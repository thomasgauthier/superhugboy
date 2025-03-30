-- Streets of Rage 2 Challenge Handler

return {
    {
        game_slug = "streetsofrage2",
        rom_path = "game_data/ROMS/Streets of Rage 2 (USA).zip",
        savestate_path = "game_data/states/Streets of Rage 2 - mini boss 1.State",
        challenge_text = "Defeat the boss!",
        challenge_text_pos = { x = 160, y = 96 },
        weight = 1,
        handler = function(state, reset)
            local playerhp_addr = 0xEFA8
            local timer_addr = 0xFC3C
            local enemyhp_addr = 0xF180
            local enemylives_addr = 0xF182
            local current_hp = mainmemory.read_s16_be(playerhp_addr)
            local current_time = mainmemory.read_u16_be(timer_addr)
            local current_enemy_hp = mainmemory.read_u16_be(enemyhp_addr)
            local current_enemy_lives = mainmemory.read_u16_be(enemylives_addr)

            if current_hp <= 0 or current_hp > 300 or current_time <= 0 then
                state.boss_spawned = nil
                return 1.0 -- Switch after 1.6 seconds (~96 frames at 60fps)
            end

            if state.boss_spawned == nil then
                state.boss_spawned = false
            end

            if not state.boss_spawned then
                if current_enemy_lives > 0 then
                    state.boss_spawned = true
                end
            end

            if state.boss_spawned and current_enemy_hp <= 0 and current_enemy_lives <= 0 then
                state.boss_spawned = nil
                return 1.6 -- Switch after 1.6 seconds (~96 frames at 60fps)
            end
            
            -- Return nil to continue the challenge
            return nil
        end
    }
}
