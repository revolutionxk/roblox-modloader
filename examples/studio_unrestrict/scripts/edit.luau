--!nocheck

local PENDING = "studio_unrestrict.pending"

task.spawn(function()
	if not game:IsLoaded() then
		game.Loaded:Wait()
	end

	if bridge.get(PENDING) ~= game.PlaceId then
		return
	end
	bridge.set(PENDING, false)

	task.wait(2)
	local played, message = pcall(function()
		PluginManager():CreatePlugin():GetPluginComponent("Actions"):ActivateAsync({
			PluginId = "RunTools",
			Category = "Actions",
			ItemId = "Play",
			DataModel = "Standalone",
		})
	end)
	if not played then
		rml.log.warn("Could not start Play in place %d: %s", game.PlaceId, tostring(message))
	end
end)
