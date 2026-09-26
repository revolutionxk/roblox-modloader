--!nocheck
-- Engine-side half of the Monaco overlay.
--
-- Everything this mod does to a script document happens here, in Luau, through the documented
-- ScriptEditorService API: opening and closing documents, reading their text, writing the editor's
-- edits back, and walking the DataModel for luau-lsp's sourcemap. The .NET half owns the WebView,
-- the Qt window and the language server, and talks to this table through two calls that never
-- yield -- `push` to queue work, `poll` to drain what happened -- so it never has to reach into
-- engine memory itself. That is what makes the mod survive a Studio update: the contract it stands
-- on is the public API, not a struct layout.

local HttpService = game:GetService("HttpService")
local ScriptEditorService = game:GetService("ScriptEditorService")

-- The walk is cooperative: it hands the engine a frame back every so many instances so a place with
-- 100k parts cannot freeze the editor while the sourcemap is built.
local SOURCEMAP_NODE_BUDGET = 2000
local SOURCEMAP_DEBOUNCE_SECONDS = 1

-- A poll that stops coming -- the .NET half died, the WebView hung -- must not grow this VM without
-- bound. Text reports coalesce per document, and past this many queued events the oldest report is
-- dropped; open/close/checked events are the ones the overlay cannot reconstruct, so they stay.
local MAX_QUEUED_EVENTS = 512

local agent = {}

agent._documents = {}
agent._idByDocument = {}
agent._events = {}
agent._nextId = 0
agent._connections = {}
agent._running = true

agent._sourcemapJson = nil
agent._sourcemapNodes = 0
agent._sourcemapWalking = false
agent._sourcemapDirty = false
agent._sourcemapPending = false
agent._pollEncodeFailures = 0

function agent._emit(event)
	if event.kind == "text" then
		-- Only the newest text for a document is worth anything: the editor replaces its buffer
		-- with it, so an older report behind it would only be overwritten.
		for index = #agent._events, 1, -1 do
			local queued = agent._events[index]
			if queued.kind == "text" and queued.id == event.id then
				table.remove(agent._events, index)
			end
		end
	end

	while #agent._events >= MAX_QUEUED_EVENTS do
		local dropped = nil

		for index = 1, #agent._events do
			if agent._events[index].kind == "text" then
				dropped = index
				break
			end
		end

		table.remove(agent._events, dropped or 1)
	end

	table.insert(agent._events, event)
end

function agent._describe(document)
	local ok, script = pcall(function()
		return document:GetScript()
	end)

	if not ok or not script then
		return nil
	end

	return script:GetFullName()
end

-- Nil, not "", when the engine cannot answer: an empty string is a document the editor would
-- happily push into Monaco and a self-check would compare against itself and pass.
function agent._textOf(document)
	local ok, text = pcall(function()
		return document:GetText()
	end)

	if not ok or type(text) ~= "string" then
		agent._emit({ kind = "error", message = `reading the document text failed: {tostring(text)}` })
		return nil
	end

	return text
end

function agent._track(document)
	local existing = agent._idByDocument[document]
	if existing then
		return existing
	end

	-- The command bar is a ScriptDocument too, and it is the one document an overlay must never
	-- take over. A document we cannot classify is left to Studio.
	local ok, isCommandBar = pcall(function()
		return document:IsCommandBar()
	end)
	if not ok or isCommandBar then
		return nil
	end

	local name = agent._describe(document)
	if not name then
		return nil
	end

	agent._nextId += 1
	local id = agent._nextId

	local entry = {
		id = id,
		document = document,
		name = name,
		queue = {},
		applied = 0,
		pumping = false,
		closed = false,
	}

	agent._documents[id] = entry
	agent._idByDocument[document] = id

	local uri = ""
	local gotUri, value = pcall(function()
		return document:GetInternalUri()
	end)
	if gotUri then
		uri = value
	end

	agent._emit({
		kind = "open",
		id = id,
		name = name,
		uri = uri,
		text = agent._textOf(document),
	})

	return id
end

function agent._untrack(document)
	local id = agent._idByDocument[document]
	if not id then
		return
	end

	local entry = agent._documents[id]
	if entry then
		entry.closed = true
		entry.queue = {}
	end

	agent._documents[id] = nil
	agent._idByDocument[document] = nil

	agent._emit({ kind = "close", id = id })
end

-- One edit at a time, newest range first. Every range the editor sends is expressed against the
-- document as the editor left it, so applying a batch bottom-up keeps the ranges that are still
-- pending valid: an edit low in the file cannot move the lines above it.
function agent._pump(entry)
	if entry.pumping then
		return
	end

	entry.pumping = true

	task.spawn(function()
		while agent._running and not entry.closed and #entry.queue > 0 do
			local edit = table.remove(entry.queue, 1)

			local ok, err = pcall(function()
				return entry.document:EditTextAsync(
					edit.text,
					edit.startLine,
					edit.startCharacter,
					edit.endLine,
					edit.endCharacter
				)
			end)

			if ok then
				entry.applied += 1
			else
				-- The editor counts the edits it is still owed, so a write that did not land has to
				-- come back as a rejection: acknowledging it would leave the two buffers apart.
				agent._emit({
					kind = "rejected",
					id = entry.id,
					count = 1,
					message = `writing an edit failed: {err}`,
				})
			end
		end

		entry.pumping = false

		if agent._running and not entry.closed then
			-- Every change reported while the queue was draining was dropped, so this is the one
			-- report the editor can trust: the document after the last write landed.
			local text = agent._textOf(entry.document)
			if text ~= nil then
				agent._emit({
					kind = "text",
					id = entry.id,
					text = text,
				})
			end
		end
	end)
end

-- Called by the .NET half for every batch of keystrokes. Returns immediately: the write itself is
-- a yielding call and lives in the pump coroutine, where the engine owns the resume.
function agent.push(payload)
	if not agent._running then
		return false
	end

	local ok, request = pcall(function()
		return HttpService:JSONDecode(payload)
	end)
	if not ok or type(request) ~= "table" then
		return false
	end

	local entry = agent._documents[request.id]
	if not entry or entry.closed then
		return false
	end

	local batch = request.edits
	if type(batch) ~= "table" or #batch == 0 then
		return false
	end

	table.sort(batch, function(left, right)
		if left.startLine ~= right.startLine then
			return left.startLine > right.startLine
		end

		return left.startCharacter > right.startCharacter
	end)

	for _, edit in batch do
		table.insert(entry.queue, edit)
	end

	agent._pump(entry)
	return true
end

-- Re-announces every document that is still open, for the Mods menu's reattach: the .NET half
-- drops its overlays and needs the open events again to build new ones.
function agent.resync()
	local announced = 0

	for id, entry in agent._documents do
		local uri = ""
		local gotUri, value = pcall(function()
			return entry.document:GetInternalUri()
		end)
		if gotUri then
			uri = value
		end

		agent._emit({
			kind = "open",
			id = id,
			name = entry.name,
			uri = uri,
			text = agent._textOf(entry.document),
		})

		announced += 1
	end

	agent._markSourcemapDirty()

	return announced
end

-- Drains everything that happened since the last call: documents opened and closed, text the engine
-- changed under us, how many of our own writes landed, and the sourcemap once a walk finishes.
function agent.poll()
	local events = agent._events

	-- Nothing is forgotten until the payload is encoded: an event the encoder chokes on (a source
	-- that is not valid UTF-8, a document past the encoder's size limit) would otherwise take the
	-- whole drained batch with it.
	local applied = {}
	for id, entry in agent._documents do
		if entry.applied > 0 then
			table.insert(applied, { id = id, count = entry.applied })
		end
	end

	local payload = {
		events = events,
		applied = applied,
	}

	if agent._sourcemapJson then
		payload.sourcemap = agent._sourcemapJson
		payload.sourcemapNodes = agent._sourcemapNodes
	end

	local encoded, json = pcall(function()
		return HttpService:JSONEncode(payload)
	end)

	if not encoded then
		agent._pollEncodeFailures += 1

		if agent._pollEncodeFailures < 2 then
			-- Could be the sourcemap, could be one event; either way the queue is still here, so
			-- let the next poll try again before anything is thrown away.
			return ""
		end

		-- Twice on the same batch is not bad luck, it is a payload that will never encode. Drop it
		-- and say so, rather than stall the overlay for the rest of the session.
		agent._events = {}
		agent._sourcemapJson = nil
		agent._pollEncodeFailures = 0

		local reported, fallback = pcall(function()
			return HttpService:JSONEncode({
				events = { { kind = "error", message = `encoding the poll payload failed: {tostring(json)}` } },
				applied = {},
			})
		end)

		return reported and fallback or ""
	end

	agent._pollEncodeFailures = 0

	-- Committed: only now is it safe to forget what the payload carries.
	if agent._events == events then
		agent._events = {}
	end

	for _, acknowledged in applied do
		local entry = agent._documents[acknowledged.id]
		if entry then
			entry.applied -= acknowledged.count
		end
	end

	agent._sourcemapJson = nil

	return json
end

-- The overlay only goes up once a real write has been proven on this Studio build: a sentinel is
-- written at the very start of the document, read back, compared, and removed again. An empty edit
-- would not do -- a build that accepts the call and drops the write passes that trivially, and a
-- dropped write is the one failure this check exists to catch.
--
-- The answer comes back as an event rather than a return value: the write yields, and a call the
-- .NET half made returns the moment the thread parks, so anything a yield produces has to be
-- reported on the next poll.
local CHECK_SENTINEL = "--[[rml-monaco-write-check]]"

function agent.check(id)
	local entry = agent._documents[id]
	if not entry then
		agent._emit({ kind = "checked", id = id, ok = false, message = "the document is no longer open" })
		return false
	end

	task.spawn(function()
		local function fail(message)
			agent._emit({ kind = "checked", id = id, ok = false, message = message })
		end

		local before = agent._textOf(entry.document)
		if before == nil then
			fail("the engine could not read the document text")
			return
		end

		local wrote, writeError = pcall(function()
			return entry.document:EditTextAsync(CHECK_SENTINEL, 1, 1, 1, 1)
		end)

		if not wrote then
			fail(tostring(writeError))
			return
		end

		local written = agent._textOf(entry.document)
		local sentinelEnd = 1 + #CHECK_SENTINEL

		if written ~= CHECK_SENTINEL .. before then
			-- The call reported success and the document does not hold what we wrote: a write this
			-- build swallows, which is exactly what the overlay must never sit on top of.
			if written ~= nil and string.sub(written, 1, #CHECK_SENTINEL) == CHECK_SENTINEL then
				pcall(function()
					return entry.document:EditTextAsync("", 1, 1, 1, sentinelEnd)
				end)
			end

			fail("the engine accepted a write but the document does not hold what was written")
			return
		end

		local cleaned, cleanError = pcall(function()
			return entry.document:EditTextAsync("", 1, 1, 1, sentinelEnd)
		end)

		if not cleaned then
			fail(`the check could not take its sentinel back out: {cleanError}`)
			return
		end

		local after = agent._textOf(entry.document)
		if after ~= before then
			fail("the check could not restore the document to the text it started from")
			return
		end

		agent._emit({ kind = "checked", id = id, ok = true, text = after })
	end)

	return true
end

-- The text the engine holds for one document. Reading is not a yielding call, so unlike a write
-- this one can answer inline -- which is what lets a regression check compare the two buffers.
function agent.text(id)
	local entry = agent._documents[id]
	if not entry then
		return nil
	end

	return agent._textOf(entry.document)
end

-- Opens a scratch script so an automated check has a document to type into without a human
-- clicking one open. Only ever called when the mod is started with RML_MONACO_SELFTEST set.
function agent.openScratch()
	task.spawn(function()
		local script = Instance.new("Script")
		script.Name = "RmlMonacoSelfTest"
		script.Source = "-- rml monaco self test\n"
		script.Parent = game:GetService("ServerScriptService")

		local ok, err = pcall(function()
			return ScriptEditorService:OpenScriptDocumentAsync(script)
		end)

		if not ok then
			agent._emit({ kind = "error", message = `opening the scratch document failed: {err}` })
		end
	end)

	return true
end

-- One timer, not one per instance: a place loading 6000 parts fires DescendantAdded 6000 times, and
-- arming a delay for each of them is 6000 closures the VM has to keep and wake.
function agent._markSourcemapDirty()
	agent._sourcemapDirty = true

	if agent._sourcemapPending or not agent._running then
		return
	end

	agent._sourcemapPending = true

	task.delay(SOURCEMAP_DEBOUNCE_SECONDS, function()
		agent._sourcemapPending = false

		if not agent._running then
			return
		end

		agent._walkSourcemap()
	end)
end

-- luau-lsp reads the place as a Rojo sourcemap: a tree of Name/ClassName/Children. Building it here
-- costs one engine call per instance instead of one interop round trip per instance.
function agent._walkSourcemap()
	if agent._sourcemapWalking or not agent._running then
		return
	end

	agent._sourcemapWalking = true
	-- Claimed now: the walk yields, so anything that changes while it runs has to mark the tree
	-- dirty again and get its own walk, or the sourcemap silently describes a place that is gone.
	agent._sourcemapDirty = false

	task.spawn(function()
		local visited = 0

		local function node(instance, name, className)
			local mapped = {
				Name = name,
				ClassName = className,
			}

			local children = {}

			for _, child in instance:GetChildren() do
				visited += 1
				if visited % SOURCEMAP_NODE_BUDGET == 0 then
					task.wait()

					if not agent._running then
						error("the data model went away", 0)
					end
				end

				table.insert(children, node(child, child.Name, child.ClassName))
			end

			if #children > 0 then
				mapped.Children = children
			end

			return mapped
		end

		local walked, tree = pcall(function()
			return node(game, "game", "DataModel")
		end)

		if walked then
			local encoded, json = pcall(function()
				return HttpService:JSONEncode(tree)
			end)

			if encoded then
				agent._sourcemapJson = json
				agent._sourcemapNodes = visited + 1
			else
				agent._emit({ kind = "error", message = `encoding the sourcemap failed: {json}` })
			end
		else
			agent._emit({ kind = "error", message = `walking the data model failed: {tree}` })
		end

		agent._sourcemapWalking = false

		if agent._sourcemapDirty and agent._running then
			agent._markSourcemapDirty()
		end
	end)
end

function agent.stop()
	agent._running = false

	for _, connection in agent._connections do
		pcall(function()
			connection:Disconnect()
		end)
	end

	agent._connections = {}
	agent._documents = {}
	agent._idByDocument = {}
	agent._events = {}
end

function agent._start()
	table.insert(
		agent._connections,
		ScriptEditorService.TextDocumentDidOpen:Connect(function(document)
			agent._track(document)
		end)
	)

	table.insert(
		agent._connections,
		ScriptEditorService.TextDocumentDidClose:Connect(function(document)
			agent._untrack(document)
		end)
	)

	table.insert(
		agent._connections,
		ScriptEditorService.TextDocumentDidChange:Connect(function(document)
			local id = agent._idByDocument[document]
			if not id then
				return
			end

			local entry = agent._documents[id]
			if not entry then
				return
			end

			-- A change reported while our own writes are still in flight describes a document the
			-- editor has already typed past; pushing it back would undo those keystrokes, and
			-- queueing the whole source only to drop it on arrival grows this VM for nothing. The
			-- pump emits the one report that can be trusted once the queue is empty.
			if entry.pumping or #entry.queue > 0 then
				return
			end

			local text = agent._textOf(document)
			if text == nil then
				return
			end

			agent._emit({
				kind = "text",
				id = id,
				text = text,
			})
		end)
	)

	table.insert(agent._connections, game.DescendantAdded:Connect(function()
		agent._markSourcemapDirty()
	end))

	table.insert(agent._connections, game.DescendantRemoving:Connect(function()
		agent._markSourcemapDirty()
	end))

	for _, document in ScriptEditorService:GetScriptDocuments() do
		agent._track(document)
	end

	agent._markSourcemapDirty()
end

agent._start()

return agent
