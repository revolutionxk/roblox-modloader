// Regression check for the Monaco overlay, meant to be the first thing run after a Studio update.
//
// It launches a Studio that has this mod installed, waits for the overlay to attach to the scratch
// document the mod opens for us, types into Monaco, and compares the editor's buffer with the text
// the engine actually holds. A Studio build that broke the write path fails here in under a minute
// instead of silently eating keystrokes in someone's session.
//
//   node tests/monaco-sync.mjs --studio C:/b/studio-monaco
//
// Options: --studio <dir> (required)   --port <cdp port, default 9223>
//          --chars <how many characters to type, default 400>   --keep (leave Studio running)
//          --place <rbxlx/rbxl to open, default a blank place this script writes>

import { spawn } from "node:child_process";
import { existsSync, mkdtempSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const args = process.argv.slice(2);
const option = (name, fallback) => {
	const at = args.indexOf(`--${name}`);
	return at === -1 ? fallback : args[at + 1];
};

const studioDir = option("studio");
const port = Number(option("port", "9223"));
const charCount = Number(option("chars", "400"));
const keepStudio = args.includes("--keep");
let place = option("place");

if (!studioDir) {
	console.error("usage: node tests/monaco-sync.mjs --studio <studio directory> [--port 9223] [--chars 400]");
	process.exit(2);
}

const executable = join(studioDir, "RobloxStudioBeta.exe");
const logDirectory = join(studioDir, "RobloxModLoader", "logs");

if (!existsSync(executable)) {
	console.error(`no Studio at '${executable}'`);
	process.exit(2);
}

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

function logText() {
	if (!existsSync(logDirectory)) return "";

	return readdirSync(logDirectory)
		.filter((name) => name.endsWith(".log"))
		.map((name) => readFileSync(join(logDirectory, name), "utf8"))
		.join("\n");
}

async function waitFor(what, predicate, timeoutMs) {
	const deadline = Date.now() + timeoutMs;

	while (Date.now() < deadline) {
		const found = await predicate();
		if (found) return found;

		await sleep(500);
	}

	throw new Error(`timed out waiting for ${what} after ${timeoutMs / 1000}s`);
}

async function findEditorTarget() {
	try {
		const response = await fetch(`http://127.0.0.1:${port}/json/list`);
		const targets = await response.json();
		return targets.find((target) => target.url.includes("rml.scripteditor"));
	} catch {
		return undefined;
	}
}

/** Minimal CDP client: one socket, Runtime.evaluate, nothing else. */
async function connect(webSocketUrl) {
	const socket = new WebSocket(webSocketUrl);
	const pending = new Map();
	let nextId = 1;

	socket.addEventListener("message", (event) => {
		const message = JSON.parse(event.data);
		const settle = pending.get(message.id);
		if (!settle) return;

		pending.delete(message.id);
		settle(message);
	});

	await new Promise((resolve, reject) => {
		socket.addEventListener("open", resolve, { once: true });
		socket.addEventListener("error", reject, { once: true });
	});

	return {
		async evaluate(expression) {
			const id = nextId++;
			const answer = new Promise((resolve) => pending.set(id, resolve));

			socket.send(JSON.stringify({
				id,
				method: "Runtime.evaluate",
				params: { expression, returnByValue: true, awaitPromise: true },
			}));

			const message = await answer;
			if (message.error) throw new Error(message.error.message);
			if (message.result?.exceptionDetails) throw new Error(message.result.exceptionDetails.text);

			return message.result.result.value;
		},
		close() {
			socket.close();
		},
	};
}

async function isPortAnswering() {
	try {
		await fetch(`http://127.0.0.1:${port}/json/version`);
		return true;
	} catch {
		return false;
	}
}

let studio;

async function run() {
	// WebView2 takes the debugging port from the environment of the Studio that owns it: if another
	// Studio already holds this port, every page we would see belongs to that one instead.
	if (await isPortAnswering()) {
		throw new Error(`something is already serving CDP on port ${port}; close that Studio or pass --port`);
	}

	// The log is append-only and a Studio started earlier may still own the file, so the run reads
	// only what is written after this point instead of clearing it.
	const baseline = logText().length;
	const runLog = () => logText().slice(baseline);

	if (!place) {
		// Studio started with no place shows the start page and never loads an Edit data model, so
		// the mod would have nothing to attach to. The smallest place Studio accepts is this one
		// line of XML; it opens with the default services and nothing else.
		place = join(mkdtempSync(join(tmpdir(), "rml-monaco-")), "blank.rbxlx");
		writeFileSync(place, '<roblox version="4"></roblox>\n');
	}

	studio = spawn(executable, ["-task", "EditFile", "-localPlaceFile", place], {
		cwd: studioDir,
		detached: false,
		stdio: "ignore",
		env: {
			...process.env,
			RML_MONACO_SELFTEST: "1",
			WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS: `--remote-debugging-port=${port}`,
		},
	});

	console.log(`studio ${studio.pid} starting`);

	await waitFor("the engine agent", () => runLog().includes("engine agent running"), 180_000);
	console.log("engine agent running");

	await waitFor("the overlay to attach", () => runLog().includes("attached editor overlay"), 120_000);
	console.log("overlay attached");

	const target = await waitFor("the Monaco page", findEditorTarget, 60_000);
	const page = await connect(target.webSocketDebuggerUrl);

	await waitFor("Monaco to load", () => page.evaluate("typeof window.__rmlVerify === 'function'"), 60_000);

	const body = "local total = 0\nfor index = 1, 10 do\n\ttotal += index * index\nend\nprint(total)\n";
	const typed = body.repeat(Math.max(1, Math.ceil(charCount / body.length)));

	await page.evaluate(`(() => {
		const editor = monaco.editor.getEditors()[0];
		editor.focus();
		const model = editor.getModel();
		editor.setPosition({ lineNumber: model.getLineCount(), column: 1 });
		for (const character of ${JSON.stringify(typed)}) editor.trigger('kb', 'type', { text: character });
		return true;
	})()`);

	console.log(`typed ${typed.length} characters`);

	await waitFor("the engine to catch up", async () => await page.evaluate("window.__rmlPending()") === 0, 60_000);

	await page.evaluate("window.__rmlVerify()");
	const engineText = await waitFor("the engine's copy of the document",
		() => page.evaluate("window.__rmlEngineText"), 30_000);
	const editorText = await page.evaluate("window.__rmlModelText()");

	if (engineText !== editorText) {
		const at = [...editorText].findIndex((character, index) => engineText[index] !== character);

		if (at === -1) {
			throw new Error("the engine and the editor agree on every character they share but not on the length: " +
				`editor has ${editorText.length}, engine has ${engineText.length}`);
		}
		throw new Error(`the engine and the editor disagree at character ${at}: ` +
			`editor has ${JSON.stringify(editorText.slice(at, at + 40))}, ` +
			`engine has ${JSON.stringify(engineText.slice(at, at + 40))}`);
	}

	// The other half of the mod: luau-lsp answering with types from the sourcemap the agent built.
	await page.evaluate(`(() => {
		const editor = monaco.editor.getEditors()[0];
		editor.focus();
		const model = editor.getModel();
		editor.setPosition({ lineNumber: model.getLineCount(), column: 1 });
		editor.trigger('kb', 'type', { text: 'workspace:Find' });
		return true;
	})()`);

	await sleep(2500);
	await page.evaluate("monaco.editor.getEditors()[0].trigger('kb','editor.action.triggerSuggest',{})");

	const suggestions = await waitFor("luau-lsp completions", async () => {
		const rows = await page.evaluate("(() => { const w = document.querySelector('.suggest-widget.visible'); return w ? [...w.querySelectorAll('.monaco-list-row')].map(r => r.textContent) : []; })()");
		return rows.length > 0 ? rows : undefined;
	}, 30_000);

	if (!suggestions.some((row) => row.includes("FindFirstChild"))) {
		throw new Error(`luau-lsp answered without the Instance API: ${JSON.stringify(suggestions.slice(0, 5))}`);
	}

	console.log(`completions: ${suggestions.length} entries, typed from the sourcemap`);

	const errors = runLog().split("\n").filter((line) => line.includes("[error]"));
	if (errors.length > 0) throw new Error(`the loader logged ${errors.length} error(s):\n${errors.join("\n")}`);

	page.close();

	console.log(`in sync: ${editorText.length} characters, no errors logged`);
}

try {
	await run();
	console.log("PASS");
} catch (failure) {
	console.error(`FAIL: ${failure.message}`);
	process.exitCode = 1;
} finally {
	if (studio && !keepStudio) studio.kill();
}
