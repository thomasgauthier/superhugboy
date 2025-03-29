-- Kirby Boss Challenge Handler

local game_slug = "kirby"
local rom_path = "game_data/ROMS/Kirby's Adventure (USA) (Rev A).zip"
return {
    {
        game_slug = game_slug,
        rom_path = rom_path,
        savestate_path = "game_data/states/Kirby's Adventure - mini boss.State",
        challenge_text = "Defeat the boss!",
        challenge_text_pos = { x = 100, y = 100 },
        weight = 1,
        handler = function(state, reset)
            local score_addr = 0x0593
            local check_addr = 0x0597
            local current_score = memory.readbyte(score_addr)
            local current_value = memory.readbyte(check_addr)
            state.prev_score = state.prev_score or current_score

            if current_score >= (state.prev_score + 100) then
                return 0.8 -- Switch after 0.8 seconds (~48 frames at 60fps)
            end

            if current_value == 255 then
                return 0.8 -- Switch after 0.8 seconds (~48 frames at 60fps)
            end
            
            state.prev_score = current_score
            
            -- Return nil to continue the challenge
            return nil
        end
    },
    {
        game_slug = game_slug,
        rom_path = rom_path,
        savestate_path = "game_data/states/Kirby's Adventure - level 1 (until door).State",
        challenge_text = "Reach the door!",
        challenge_text_pos = { x = 100, y = 100 },
        weight = 1,
        handler = function(state, reset)
            local check_addr = 0x058E
            local current_value = memory.readbyte(check_addr)
            local current_state = memory.readbyte(0x0597)
            state.prev_value = state.prev_value or current_value

            if state.prev_value ~= 32 and current_value == 32 then
                return 0.016 -- Switch immediately (~1 frame)
            end
            state.prev_value = current_value

            if current_state == 255 then
                return 0.8 -- Switch after 0.8 seconds (~48 frames at 60fps)
            end
            
            -- Return nil to continue the challenge
            return nil
        end
    }
}
