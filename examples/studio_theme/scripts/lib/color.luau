--!nocheck

local color = {}

function color.toHex(value)
	return "#" .. value:ToHex():upper()
end

function color.fromHex(text)
	if type(text) ~= "string" then
		return nil
	end
	local digits = text:match("^#?(%x%x%x%x%x%x)$")
	return digits and Color3.fromHex(digits) or nil
end

return color
