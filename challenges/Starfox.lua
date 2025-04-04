return {
    {
        game_slug = "starfox",
        rom_path = "game_data/ROMS/Star Fox (USA).zip",
        savestate_path = "game_data/states/Star Fox - boss.State",
        challenge_text = "Beat the boss!",
        challenge_text_pos = { x = 128, y = 96 },
        weight = 1,
        handler = function(state, reset)
            local player_hp = mainmemory.read_u8(0x000396)
            local boss_defeated = mainmemory.read_u16_le(0x0014AC)


            if player_hp == 0 then
                return 1.8 -- Switch after 1.6 seconds (~96 frames at 60fps)
            end

            if boss_defeated == 31 then
                return 3.2 -- Switch after 1.6 seconds (~96 frames at 60fps)
            end
            
            -- Return nil to continue the challenge
            return nil
        end
    }
}
