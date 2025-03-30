-- Links Awakening Boss Challenge Handler

return {
    {
        game_slug = "awakening_boss",
        rom_path = "game_data/ROMS/Legend of Zelda, The - Link's Awakening DX (USA, Europe) (SGB Enhanced).zip",
        savestate_path = "game_data/states/Link's Awakening - mini boss.State",
        challenge_text = "Defeat the boss!",
        challenge_text_pos = { x = 80, y = 32 },
        weight = 1,
        handler = function(state, reset)
            local enemy_hp = mainmemory.readbyte(0x364)
            local player_hp = mainmemory.readbyte(0x1B5A)

            if enemy_hp <= 0 then
                return 2.87 -- Equivalent to 172 frames at 60fps
            end

            if player_hp <= 0 then
                return 1.6 -- Equivalent to 96 frames at 60fps
            end
            
            -- Return nil to continue the challenge
            return nil
        end
    }
}
