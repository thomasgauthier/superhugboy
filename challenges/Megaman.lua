-- BombMan Challenge Handler


local game_slug = "megaman"
local rom_path =  "game_data/ROMS/Mega Man (USA).zip"
local challenge_text = "Finish the screen!"

local handler = function(state, reset)
    local camera_state_addr = 0x001C
    local hp_addr = 0x006A
    local zero_check_addresses = {0x00E0, 0x01FA, 0x0500, 0x0501, 0x051F, 0x0520, 0x053E, 0x053F, 0x055D, 0x055E}
    
    local camera_state = memory.readbyte(camera_state_addr)
    local current_hp = memory.readbyte(hp_addr)

    state.camera_stable_frames = state.camera_stable_frames or 0
    state.prev_hp = state.prev_hp or current_hp

    if camera_state == 0x02 then
        state.camera_stable_frames = state.camera_stable_frames + 1
        if state.camera_stable_frames > 3 then
            -- Modified to return a switch time instead of calling a function
            return 0.016 -- Switch immediately
        end
    else
        state.camera_stable_frames = 0
    end

    if state.prev_hp > 0 and current_hp == 0 then
        -- Modified to return a switch time instead of scheduling
        return 0.8 -- Switch after 0.8 seconds (~48 frames at 60fps)
    end

    local all_zero = true
    for _, addr in ipairs(zero_check_addresses) do
        if memory.readbyte(addr) ~= 0 then
            all_zero = false
            break
        end
    end
    if all_zero then
        -- Modified to return a switch time instead of scheduling
        return 0.8 -- Switch after 0.8 seconds (~48 frames at 60fps)
    end
    
    state.prev_hp = current_hp
    return nil -- Continue the challenge
end

return {
    {
        game_slug = game_slug,
        rom_path = rom_path,
        savestate_path = "game_data/states/Mega Man - bomb man.State",
        challenge_text = challenge_text,
        challenge_text_pos = { x = 128, y = 96 },
        weight = 1,
        handler = handler
    },
    {
        game_slug = game_slug,
        rom_path = rom_path,
        savestate_path = "game_data/states/Mega Man - fire man.State",
        challenge_text = challenge_text,
        challenge_text_pos = { x = 128, y = 96 },
        weight = 1,
        handler = handler
    },
    {
        game_slug = game_slug,
        rom_path = rom_path,
        savestate_path = "game_data/states/Mega Man - cut man.State",
        challenge_text = challenge_text,
        challenge_text_pos = { x = 128, y = 96 },
        weight = 1,
        handler = handler
    }
} 