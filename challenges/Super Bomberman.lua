
return {
    {
        game_slug = "superbomberman",
        rom_path = "game_data/ROMS/Super Bomberman (USA).zip",
        savestate_path = "game_data/states/Super Bomberman - level 1.State",
        challenge_text = "Finish the level!",
        challenge_text_pos = { x = 128, y = 96 },
        weight = 1,
        handler = function(state, reset)
            local game_state = mainmemory.read_u8(0x002804)
            local player_lives = mainmemory.read_u8(0x000D7D)

            if game_state ~= 0 then
                return 1.6 -- Switch after 1.6 seconds (~96 frames at 60fps)
            end

            if player_lives == 4 then
                return 0 -- Switch after 1.6 seconds (~96 frames at 60fps)
            end
            
            -- Return nil to continue the challenge
            return nil
        end
    }
}
