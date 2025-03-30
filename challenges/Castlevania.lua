-- Castlevania Challenge Handler

return {
    {
        game_slug = "castlevania",
        rom_path = "game_data/ROMS/Castlevania (USA) (Rev A).zip",
        savestate_path = "game_data/states/Castlevania - level 1.State",
        challenge_text = "Finish the screen!",
        challenge_text_pos = { x = 128, y = 96 },
        weight = 1,
        handler = function(state, reset)
            local trigger_addr = 0x0018
            local trigger_value = memory.readbyte(trigger_addr)
            local death_addr = 0x0045
            local death_value = memory.readbyte(death_addr)

            state.prev_trigger_value = state.prev_trigger_value or trigger_value
            state.prev_death_value = state.prev_death_value or death_value

            if state.prev_trigger_value ~= 8 and trigger_value == 8 then
                -- Return to switch immediately
                return 1.0 -- Switch after ~1 frame
            end

            if state.prev_death_value ~= 0 and death_value == 0 then
                -- Return to switch after delay
                return 0.8 -- Switch after 0.8 seconds (roughly 48 frames at 60fps)
            end

            state.prev_trigger_value = trigger_value
            state.prev_death_value = death_value
            
            -- Return nil to continue the challenge
            return nil
        end
    }
}
