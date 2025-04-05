local game_slug = "mario1"
local rom_path = "game_data/ROMS/Super Mario Bros. (Japan, USA).zip"
local death_transition_delay = 2.78

function check_mario_fail()
    local player_state = mainmemory.readbyte(0x000E)
    local sfx_id = mainmemory.readbyte(0x0712)
    local timer1 = mainmemory.readbyte(0x07F8)
    local timer2 = mainmemory.readbyte(0x07F9)
    local timer3 = mainmemory.readbyte(0x07FA)
    local time_is_up = timer1 == 0 and timer2 == 0 and timer3 == 0
    
    return player_state == 11 or sfx_id == 1 or time_is_up
end


return {
    {
        game_slug = game_slug,
        rom_path = rom_path,
        savestate_path = "game_data/states/Super Mario Bros - castle.State",
        challenge_text = "Defeat Bowser!",
        challenge_text_pos = { x = 128, y = 96 },
        weight = 0.5,
        handler = function(state, reset)
            -- Check enemy types in memory
            local enemy_types = {
                memory.readbyte(0x0016),
                memory.readbyte(0x0017),
                memory.readbyte(0x0018),
                memory.readbyte(0x0019),
                memory.readbyte(0x001A)
            }
            
            local are_enemies_present = false
            for k,v in pairs(enemy_types) do
                are_enemies_present = are_enemies_present or enemy_types[k] > 0
            end
            

            -- If Mario died/failed, return time to switch
            if check_mario_fail() then
                return death_transition_delay -- Switch after 0.8 seconds (~48 frames at 60fps)
            end
            
            -- If no enemies are present, victory condition met
            if not are_enemies_present then
                return 0.016 -- Switch after 0.016 seconds (~1 frame at 60fps)
            end
            
            -- Return nil to continue the challenge
            return nil
        end
    },
    {
        game_slug = game_slug,
        rom_path = rom_path,
        savestate_path = "game_data/states/Super Mario Bros - 1-1.State",
        challenge_text = "Beat the level!",
        challenge_text_pos = { x = 128, y = 96 },
        weight = 0.5,
        handler = function(state, reset)
            local player_state = mainmemory.readbyte(0x000E)

            -- If Mario died/failed, schedule switch
            if check_mario_fail() then
                return death_transition_delay -- Switch after 0.8 seconds (~48 frames at 60fps)
            end

            -- If player_state is 4 (level completed), schedule switch
            if player_state == 4 then
                return 0.8 -- Switch after 0.8 seconds (~48 frames at 60fps)
            end
            
            -- Return nil to continue the challenge
            return nil
        end
    }
} 