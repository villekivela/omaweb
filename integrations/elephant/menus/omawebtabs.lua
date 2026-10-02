-- Omaweb's tabs as an Elephant menu, so Walker can list them and take the reader to one
-- (ADR 0057). Elephant reads menus from ~/.config/elephant/menus/, where Omaweb links this file.
--
-- The tabs are asked for when the menu opens, never cached: a tab opened a second ago is one the
-- reader may be looking for. `omaweb tabs` answers from the running browser and never starts one.

Name = "omawebtabs"
NamePretty = "Omaweb tabs"
Icon = "omaweb"
Cache = false
Action = "lua:Focus"
HideFromProviderlist = false
Description = "Switch to a tab in Omaweb"
SearchName = false

-- A title is a page's own text, so it can hold anything a row cannot show.
local function oneLine(text)
    return (text:gsub("[%c%s]+", " "):gsub("^%s+", ""):gsub("%s+$", ""))
end

-- The host an address is on, without any user name or password it carries.
local function hostOf(address)
    local authority = address:match("^%a[%w+.-]*://([^/?#]*)")
    if not authority then
        return ""
    end
    return (authority:gsub("^.*@", ""))
end

local function say(text, detail)
    return { { Text = text, Subtext = detail, Value = "" } }
end

function GetEntries()
    local handle = io.popen("omaweb tabs --all --json 2>/dev/null")
    if not handle then
        return say("Omaweb is not running", "Start Omaweb to list its tabs here")
    end
    local output = handle:read("*a")
    handle:close()
    local answer = output and output ~= "" and jsonDecode(output) or nil
    if type(answer) ~= "table" then
        return say("Omaweb is not running", "Start Omaweb to list its tabs here")
    end
    if not answer.ok then
        return say("Omaweb could not list its tabs", oneLine(tostring(answer.error or "")))
    end

    local entries = {}
    for _, tab in ipairs(answer.tabs or {}) do
        local address = tostring(tab.url or "")
        local title = oneLine(tostring(tab.title or ""))
        local host = hostOf(address)
        local space = oneLine(tostring(tab.spaceName or ""))
        local where = space
        if host ~= "" then
            where = host .. " · " .. space
        end
        table.insert(entries, {
            Text = title ~= "" and title or address,
            Subtext = where,
            Value = tostring(tab.id),
            -- The whole address is searched, not only the host shown.
            Keywords = { address },
        })
    end
    return entries
end

-- Elephant passes the entry's Value. Omaweb selects the tab, switching Space if need be, and
-- brings its window forward.
function Focus(value)
    if value == nil or value == "" then
        return
    end
    os.execute("omaweb focus -- '" .. (value:gsub("'", "'\\''")) .. "'")
end
