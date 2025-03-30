-- Tetris Challenge Handler

return {
    {
        game_slug = "tetris",
        rom_path = "game_data/ROMS/Tetris (USA).zip",
        savestate_path = "game_data/states/Tetris.State",
        challenge_text = "Make 1 line!",
        challenge_text_pos = { x = 128, y = 96 },
        weight = 1,
        handler = function(state, reset)
            local game_phase_addr = 0x0048
            local game_phase = memory.readbyte(game_phase_addr)
            local game_end_addr = 0x0058
            local game_end = memory.readbyte(game_end_addr)

            state.prev_game_phase = state.prev_game_phase or game_phase
            state.game_end = state.game_end or game_end

            if state.prev_game_phase ~= 4 and game_phase == 4 then
                return 1.07 -- Switch after ~64 frames at 60fps
            end

            if state.game_end == 0 and game_end > 0 then
                return 1.07 -- Switch after ~64 frames at 60fps
            end

            state.prev_game_phase = game_phase
            state.game_end = game_end
            
            -- Return nil to continue the challenge
            return nil
        end
    }
}
