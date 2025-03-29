-- Street Fighter 2 Challenge Handler


local handler = function(state, reset)
    local player_hp = mainmemory.read_u16_le(0x000636)
    local opponent_hp = mainmemory.read_u16_le(0x000836)
    local in_fight = mainmemory.read_u16_le(0x0000E0)

    if in_fight == 7 and player_hp <= 0 then
        return 1.6 -- Switch after 1.6 seconds (~96 frames at 60fps)
    end

    if in_fight == 7 and opponent_hp <= 0 then
        return 1.6 -- Switch after 1.6 seconds (~96 frames at 60fps)
    end
    
    -- Return nil to continue the challenge
    return nil
end

return {
    {
        game_slug = "sf2",
        rom_path = "game_data/ROMS/Street Fighter II Turbo (USA) (Rev 1).zip",
        savestate_path = "game_data/states/Street Fighter II Turbo - blanka vs dhalsim.State",
        challenge_text = "Win the fight!",
        challenge_text_pos = { x = 100, y = 100 },
        weight = 1,
        handler = handler
    },
    {
        game_slug = "sf2",
        rom_path = "game_data/ROMS/Street Fighter II Turbo (USA) (Rev 1).zip",
        savestate_path = "game_data/states/Street Fighter II Turbo - ryu vs guile.State",
        challenge_text = "Win the fight!",
        challenge_text_pos = { x = 100, y = 100 },
        weight = 1,
        handler = handler
    },
}
