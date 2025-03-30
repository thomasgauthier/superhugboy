-- Pokemon Challenge Handler

return {
    {
        game_slug = "pokemon",
        rom_path = "game_data/ROMS/Pokemon - Red Version (USA, Europe) (SGB Enhanced).zip",
        savestate_path = "game_data/states/Pokemon Red - choose pokemon.State",
        challenge_text = "Choose a pokémon!",
        challenge_text_pos = { x = 80, y = 32 },
        weight = 1,
        handler = function(state, reset)
            local pokemon_in_team = mainmemory.readbyte(0x1163)

            if pokemon_in_team >= 1 then
                -- Print info to console for debugging
                print("Pokemon in team: " .. pokemon_in_team)
                
                -- Return immediate switch time
                return 0.016 -- Switch immediately (~1 frame)
            end
            
            -- Return nil to continue the challenge
            return nil
        end
    }
}
