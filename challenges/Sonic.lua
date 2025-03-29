-- Sonic Challenge Handler

return {
    {
        game_slug = "sonic",
        rom_path = "game_data/ROMS/Sonic The Hedgehog (USA, Europe).zip",
        savestate_path = "game_data/states/Sonic The Hedgehog - level 1.State",
        challenge_text = "Beat the level!",
        challenge_text_pos = { x = 100, y = 100 },
        weight = 1,
        handler = function(state, reset)
            local lives = mainmemory.read_s16_be(0xFE12)
            local score_bonus = mainmemory.read_s16_be(0xF7D2)
            
            -- Initialize previous lives in state if it doesn't exist
            if state.previous_lives == nil then
                state.previous_lives = lives
            end
            
            -- Check if lives decreased
            if lives < state.previous_lives then
                return 0.016 -- Switch immediately (~1 frame)
            end
            
            -- Update previous lives for next check
            state.previous_lives = lives

            if score_bonus > 0 then
                return 0.016 -- Switch immediately (~1 frame)
            end
            
            -- Return nil to continue the challenge
            return nil
        end
    }
}
