-- Challenge Manager for SMB3 (BizHawk Lua)

-- Define savestates
local savestates = {
    --["1-1"] = "game_data/states/1-1.state",
    ["mario3_first_mini_boss"] = "game_data/states/Super Mario Bros. 3 - first mini boss.State",
    ["mario_1-1"] = "game_data/states/Super Mario Bros - 1-1.State",
    ["mario_castle"] = "game_data/states/Super Mario Bros - castle.State",
    ["bombman"] = "game_data/states/Mega Man - bomb man.State",
    ["fireman"] = "game_data/states/Mega Man - fire man.State", 
    ["cutman"] = "game_data/states/Mega Man - cut man.State",
    ["zelda_boss"] = "game_data/states/Legend of Zelda - boss 1.State",
    ["zelda_take_this"] = "game_data/states/Legend of Zelda - take this.State",
    ["awakening_boss"] = "game_data/states/Link's Awakening - mini boss.State",
    ["alttp_cell"] = "game_data/states/A Link to the Past - mini boss.State",
    ["castlevania"] = "game_data/states/Castlevania - level 1.State",
    ["tetris"] = "game_data/states/Tetris.State",
    ["pinball"] = "game_data/states/pinball.State",
    ["donkeykong"] = "game_data/states/Donkey Kong Country - level 1.State",
    ["kirby_boss"] = "game_data/states/Kirby's Adventure - mini boss.State",
    ["kirby_level1"] = "game_data/states/Kirby's Adventure - level 1 (until door).State",
    ["mario3_spikes"] = "game_data/states/Super Mario Bros. 3 - crushing ceiling.State",
    ["streetsofrage2"] = "game_data/states/Streets of Rage 2 - mini boss 1.State",
    ["sonic"] = "game_data/states/Sonic The Hedgehog - level 1.State",
    ["pokemon"] = "game_data/states/Pokemon Red - choose pokemon.State",
    ["supermetroid_escape"] = "game_data/states/Super Metroid - First Escape.State",
    ["sf2_blanka"] = "game_data/states/Street Fighter II Turbo - blanka vs dhalsim.State",
    ["sf2_ryu"] = "game_data/states/Street Fighter II Turbo - ryu vs guile.State"
}

local challenge_roms = {
    --["1-1"] = "game_data/ROMS/Super Mario Bros. 3 (USA) (Rev 1).zip",
    ["mario3_first_mini_boss"] = "game_data/ROMS/Super Mario Bros. 3 (USA) (Rev 1).zip",
    ["mario_1-1"] = "game_data/ROMS/Super Mario Bros. (Japan, USA).zip",
    ["mario_castle"] = "game_data/ROMS/Super Mario Bros. (Japan, USA).zip",
    ["bombman"] = "game_data/ROMS/Mega Man (USA).zip",
    ["fireman"] = "game_data/ROMS/Mega Man (USA).zip",
    ["cutman"] = "game_data/ROMS/Mega Man (USA).zip",
    ["zelda_boss"] = "game_data/ROMS/Legend of Zelda, The (USA) (Rev 1).zip",
    ["zelda_take_this"] = "game_data/ROMS/Legend of Zelda, The (USA) (Rev 1).zip",
    ["awakening_boss"] = "game_data/ROMS/Legend of Zelda, The - Link's Awakening DX (USA, Europe) (SGB Enhanced).zip",
    ["alttp_cell"] = "game_data/ROMS/Legend of Zelda, The - A Link to the Past (USA).zip",
    ["castlevania"] = "game_data/ROMS/Castlevania (USA) (Rev A).zip",
    ["tetris"] = "game_data/ROMS/Tetris (USA).zip",
    ["pinball"] = "game_data/ROMS/Pokemon Pinball - Ruby & Sapphire (USA).zip",
    ["donkeykong"] = "game_data/ROMS/Donkey Kong Country (USA) (Rev 2).zip",
    ["kirby_boss"] = "game_data/ROMS/Kirby's Adventure (USA) (Rev A).zip",
    ["kirby_level1"] = "game_data/ROMS/Kirby's Adventure (USA) (Rev A).zip",
    ["mario3_spikes"] = "game_data/ROMS/Super Mario Bros. 3 (USA) (Rev 1).zip",
    ["streetsofrage2"] = "game_data/ROMS/Streets of Rage 2 (USA).zip",
    ["sonic"] = "game_data/ROMS/Sonic The Hedgehog (USA, Europe).zip",
    ["pokemon"] = "game_data/ROMS/Pokemon - Red Version (USA, Europe) (SGB Enhanced).zip",
    ["supermetroid_escape"] = "game_data/ROMS/Super Metroid (Japan, USA) (En,Ja).zip",
    ["sf2_blanka"] = "game_data/ROMS/Street Fighter II Turbo (USA) (Rev 1).zip",
    ["sf2_ryu"] = "game_data/ROMS/Street Fighter II Turbo (USA) (Rev 1).zip"
}

local challenge_names = {
    --["1-1"] = "1-1",
    ["mario3_first_mini_boss"] = "Beat the boss!",
    ["mario_1-1"] = "Beat the level!",
    ["mario_castle"] = "Defeat Bowser!",
    ["bombman"] = "Finish the screen!",
    ["fireman"] = "Finish the screen!", 
    ["cutman"] = "Finish the screen!",
    ["zelda_boss"] = "Beat the boss!",
    ["zelda_take_this"] = "",
    ["awakening_boss"] = "Defeat the boss!",
    ["alttp_cell"] = "Free the princess!",
    ["castlevania"] = "Finish the screen!",
    ["tetris"] = "Make 1 line!",
    ["pinball"] = "Pinball",
    ["donkeykong"] = "Finish the level!",
    ["kirby_boss"] = "Beat the boss!",
    ["kirby_level1"] = "Reach the door!",
    ["mario3_spikes"] = "Reach the door!",
    ["streetsofrage2"] = "Beat the boss!",
    ["sonic"] = "Beat the level!",
    ["pokemon"] = "Choose a pokémon!",
    ["supermetroid_escape"] = "",
    ["sf2_blanka"] = "Win the fight!",
    ["sf2_ryu"] = "Win the fight!"
}

local current_challenge = {
    name = nil,
    handler = nil,
    state = {}
}

local completed_challenges = {}

local scheduled_switch = {
    active = false,
    frames_left = 0,
    target_challenge = nil
}

local challenge_handlers = {}

challenge_handlers["supermetroid_escape"] = function(state)
    local game_state = mainmemory.read_u16_le(0x000998)

    if game_state == 32 then
        schedule_challenge_switch(96, nil)
    end

    if game_state == 35 then
        schedule_challenge_switch(96, nil)
    end
end

challenge_handlers["alttp_cell"] = function(state)
    local music_id = mainmemory.read_u16_le(0x000132)
    local player_state = mainmemory.read_u16_le(0x00005E)

    if music_id == 61712 or player_state == 2 then
        schedule_challenge_switch(96, nil)
    end

    if music_id == 6416 then
        schedule_challenge_switch(96, nil)
    end
end


challenge_handlers["awakening_boss"] = function(state)
    local enemy_hp = mainmemory.readbyte(0x364)
    local player_hp = mainmemory.readbyte(0x1B5A)

    if enemy_hp <= 0 then
        schedule_challenge_switch(172, nil)
    end

    if player_hp <= 0 then
        schedule_challenge_switch(96, nil)
    end
end

challenge_handlers["mario_1-1"] = function(state)
    local player_state = mainmemory.readbyte(0x000E)

    if check_mario_fail() then
        schedule_challenge_switch(48, nil)
    end

    if player_state == 4 then
        schedule_challenge_switch(48, nil)
    end
end

challenge_handlers["mario_castle"] = function(state)
    local enemy_types = {
        mainmemory.readbyte(0x0016),
        mainmemory.readbyte(0x0017),
        mainmemory.readbyte(0x0018),
        mainmemory.readbyte(0x0019),
        mainmemory.readbyte(0x001A)
    }
    local are_enemies_present = false
    for k,v in pairs(enemy_types) do
        are_enemies_present = are_enemies_present or enemy_types[k] > 0
    end

    if check_mario_fail() then
        schedule_challenge_switch(48, nil)
    end

    if not are_enemies_present then
        schedule_challenge_switch(1, nil)
    end
end

function check_mario_fail()
    local player_state = mainmemory.readbyte(0x000E)
    local sfx_id = mainmemory.readbyte(0x0712)
    local timer1 = mainmemory.readbyte(0x07F8)
    local timer2 = mainmemory.readbyte(0x07F9)
    local timer3 = mainmemory.readbyte(0x07FA)
    local time_is_up = timer1 == 0 and timer2 == 0 and timer3 == 0
    
    return player_state == 11 or sfx_id == 1 or time_is_up
end

challenge_handlers["sf2_ryu"] = function(state)
    local player_hp = mainmemory.read_u16_le(0x000636)
    local opponent_hp = mainmemory.read_u16_le(0x000836)
    local in_fight = mainmemory.read_u16_le(0x0000E0)

    if in_fight == 7 and player_hp <= 0 then
        schedule_challenge_switch(96, nil)
    end

    if in_fight == 7 and opponent_hp <= 0 then
        schedule_challenge_switch(96, nil)
    end
end

challenge_handlers["sf2_blanka"] = challenge_handlers["sf2_ryu"]

challenge_handlers["pokemon"] = function(state)
    local pokemon_in_team = mainmemory.readbyte(0x1163)

    if pokemon_in_team >= 1 then
        print(pokemon_in_team)
        switch_to_random_challenge(current_challenge.name)
    end
end

challenge_handlers["sonic"] = function(state)
    local lives = mainmemory.read_s16_be(0xFE12)
    local score_bonus = mainmemory.read_s16_be(0xF7D2)

    if lives <= 2 then
        schedule_challenge_switch(96, nil)
    end

    if score_bonus > 0 then
        switch_to_random_challenge(current_challenge.name)
    end
end

challenge_handlers["streetsofrage2"] = function(state)
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
        schedule_challenge_switch(96, nil)
        return
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
        schedule_challenge_switch(96, nil)
        return
    end
end

challenge_handlers["kirby_boss"] = function(state)
    local score_addr = 0x0593
    local check_addr = 0x0597
    local current_score = memory.readbyte(score_addr)
    local current_value = memory.readbyte(check_addr)
    state.prev_score = state.prev_score or current_score

    if current_score >= (state.prev_score + 100) then
        schedule_challenge_switch(48, nil)
        return
    end

    if current_value == 255 then
        schedule_challenge_switch(48, nil)
        return
    end
    state.prev_score = current_score
end

challenge_handlers["kirby_level1"] = function(state)
    local check_addr = 0x058E
    local current_value = memory.readbyte(check_addr)
    local current_state = memory.readbyte(0x0597)
    state.prev_value = state.prev_value or current_value

    if state.prev_value ~= 32 and current_value == 32 then
        switch_to_random_challenge(current_challenge.name)
        return
    end
    state.prev_value = current_value

    if current_state == 255 then
        schedule_challenge_switch(48, nil)
        return
    end
end


function check_death_and_switch(state)
    local fanfare_play_byte = 0x04F4
    local death_fanfare_value = 1
    local current_fanfare = memory.readbyte(fanfare_play_byte)

    if state.prev_fanfare == nil then
        state.prev_fanfare = current_fanfare
    end

    if state.prev_fanfare ~= death_fanfare_value and current_fanfare == death_fanfare_value then
        print("Mario died! Switching to a random challenge...")
        state.prev_fanfare = current_fanfare
        return true
    end
    state.prev_fanfare = current_fanfare
    return false
end

challenge_handlers["1-1"] = function(state)
    local mario_x_addr = 0x0090
    local mario_x = memory.readbyte(mario_x_addr)
    
    if state.prev_x == nil then
        state.prev_x = mario_x
    end

    if state.prev_x <= 244 and mario_x > 244 then
        switch_to_random_challenge(current_challenge.name)
        return
    end

    if check_death_and_switch(state) then
        schedule_challenge_switch(48, nil)
        return
    end
    state.prev_x = mario_x
end


challenge_handlers["mario3_first_mini_boss"] = function(state)
    local trigger_addr = 0x05F3
    local trigger_value = memory.readbyte(trigger_addr)

    if state.prev_trigger_value == nil then
        state.prev_trigger_value = trigger_value
    end

    if state.prev_trigger_value ~= 0x01 and trigger_value == 0x01 then
        switch_to_random_challenge(current_challenge.name)
        return
    end

    if check_death_and_switch(state) then
        schedule_challenge_switch(48, nil)
        return
    end
    state.prev_trigger_value = trigger_value
end

challenge_handlers["mario3_spikes"] = function(state)
    local check_addr = 0x0075
    local current_value = memory.readbyte(check_addr)
    state.prev_value = state.prev_value or current_value

    if state.prev_value ~= 7 and current_value == 7 then
        switch_to_random_challenge(current_challenge.name)
        return
    end

    if check_death_and_switch(state) then
        schedule_challenge_switch(48, nil)
        return
    end
    state.prev_value = current_value
end

challenge_handlers["bombman"] = function(state)
    local camera_state_addr = 0x001C
    local hp_addr = 0x006A
    local zero_check_addresses = {0x00E0, 0x01FA, 0x0500, 0x0501, 0x051F, 0x0520, 0x053E, 0x053F, 0x055D, 0x055E}
    
    local camera_state = memory.readbyte(camera_state_addr)
    local current_hp = memory.readbyte(hp_addr)

    state.camera_stable_frames = state.camera_stable_frames or 0
    state.prev_hp = state.prev_hp or current_hp

    if camera_state == 0x02 then
        state.camera_stable_frames = state.camera_stable_frames + 1
        if state.camera_stable_frames > 3 then
            switch_to_random_challenge(current_challenge.name)
            return
        end
    else
        state.camera_stable_frames = 0
    end

    if state.prev_hp > 0 and current_hp == 0 then
        schedule_challenge_switch(48, nil)
        return
    end

    local all_zero = true
    for _, addr in ipairs(zero_check_addresses) do
        if memory.readbyte(addr) ~= 0 then
            all_zero = false
            break
        end
    end
    if all_zero then
        schedule_challenge_switch(48, nil)
        return
    end
    state.prev_hp = current_hp
end

challenge_handlers["fireman"] = challenge_handlers["bombman"]
challenge_handlers["cutman"] = challenge_handlers["bombman"]

challenge_handlers["zelda_take_this"] = function(state)
    -- Address to check for the "take this" event
    local check_addr = 0x0657
    local current_value = memory.readbyte(check_addr)
    
    -- Initialize prev_value if it doesn't exist
    if state.prev_value == nil then
        state.prev_value = current_value
    end

    -- Check for transition from 0 to non-zero
    if state.prev_value == 0 and current_value ~= 0 then
        schedule_challenge_switch(64, nil)
        return
    end

    -- Store current value for next comparison
    state.prev_value = current_value
end

challenge_handlers["zelda_boss"] = function(state)
    local health_addr = 0x066F
    local sub_hp_addr = 0x0670
    local room_addr = 0x00EB
    
    local byte_value = memory.readbyte(health_addr)
    local heart_container_count = (byte_value & 0xF0) >> 4
    local filled_hearts = byte_value & 0x0F
    local sub_hp = memory.readbyte(sub_hp_addr)
    local current_room_value = memory.readbyte(room_addr)

    state.prev_heart_container_count = state.prev_heart_container_count or heart_container_count
    state.prev_filled_hearts = state.prev_filled_hearts or filled_hearts
    state.prev_sub_hp = state.prev_sub_hp or sub_hp
    state.prev_room_value = state.prev_room_value or current_room_value

    if (state.prev_filled_hearts > 0 or state.prev_sub_hp > 0) and filled_hearts == 0 and sub_hp == 0 then
        schedule_challenge_switch(48, nil)
        return
    end

    if heart_container_count > state.prev_heart_container_count then
        switch_to_random_challenge(current_challenge.name)
        return
    end

    if state.prev_room_value ~= 69 and current_room_value == 69 then
        switch_challenge(current_challenge.name, "game_data/states/Legend of Zelda - boss 1 - reset.State")
        return
    end

    state.prev_heart_container_count = heart_container_count
    state.prev_filled_hearts = filled_hearts
    state.prev_sub_hp = sub_hp
    state.prev_room_value = current_room_value
end

challenge_handlers["castlevania"] = function(state)
    local trigger_addr = 0x0018
    local trigger_value = memory.readbyte(trigger_addr)
    local death_addr = 0x0045
    local death_value = memory.readbyte(death_addr)

    state.prev_trigger_value = state.prev_trigger_value or trigger_value
    state.prev_death_value = state.prev_death_value or death_value

    if state.prev_trigger_value ~= 8 and trigger_value == 8 then
        switch_to_random_challenge(current_challenge.name)
        return
    end

    if state.prev_death_value ~= 0 and death_value == 0 then
        schedule_challenge_switch(48, nil)
        return
    end

    state.prev_trigger_value = trigger_value
    state.prev_death_value = death_value
end

challenge_handlers["tetris"] = function(state)
    local game_phase_addr = 0x0048
    local game_phase = memory.readbyte(game_phase_addr)
    local game_end_addr = 0x0058
    local game_end = memory.readbyte(game_end_addr)

    state.prev_game_phase = state.prev_game_phase or game_phase
    state.game_end = state.game_end or game_end

    if state.prev_game_phase ~= 4 and game_phase == 4 then
        schedule_challenge_switch(64, nil)
    end

    if state.game_end == 00 and game_end > 0 then
        schedule_challenge_switch(64, nil)
    end

    state.prev_game_phase = game_phase
    state.game_end = game_end
end

challenge_handlers["pinball"] = function(state)
    local game_phase_addr = 0x0029AC
    local game_phase = memory.readbyte(game_phase_addr, "EWRAM")
    
    state.prev_game_phase = state.prev_game_phase or game_phase

    if state.prev_game_phase ~= 1 and game_phase == 1 then
        schedule_challenge_switch(64, nil)
    end
    
    state.prev_game_phase = game_phase
end

challenge_handlers["donkeykong"] = function(state)
    local game_state_addr = 0x0040
    local lives_addr = 0x0575
    local game_state = memory.readbyte(game_state_addr)
    local lives = memory.readbyte(lives_addr)

    state.prev_game_state = state.prev_game_state or game_state
    state.prev_lives = state.prev_lives or lives

    if state.prev_game_state ~= 12 and game_state == 12 then
        switch_to_random_challenge(current_challenge.name)
        return
    end

    if lives < state.prev_lives then
        schedule_challenge_switch(48, nil)
        return
    end

    state.prev_game_state = game_state
    state.prev_lives = lives
end

function get_challenge_keys()
    local keys = {"supermetroid_escape", "alttp_cell"}
    -- for k, _ in pairs(challenge_handlers) do
    --     table.insert(keys, k)
    -- end
    return keys
end

function schedule_challenge_switch(frames, challenge_name)
    scheduled_switch.active = true
    scheduled_switch.frames_left = frames
    scheduled_switch.target_challenge = challenge_name
end

function switch_to_random_challenge(current_challenge_name)
    local available_challenges = get_challenge_keys()
    local filtered_challenges = {}
    
    for _, name in ipairs(available_challenges) do
        if not completed_challenges[name] and name ~= current_challenge_name then
            table.insert(filtered_challenges, name)
        end
    end

    if #filtered_challenges == 0 then
        completed_challenges = {}
        for _, name in ipairs(available_challenges) do
            if name ~= current_challenge_name then
                table.insert(filtered_challenges, name)
            end
        end
    end

    if #filtered_challenges > 0 then
        local random_index = math.random(1, #filtered_challenges)
        local new_challenge = filtered_challenges[random_index]
        completed_challenges[current_challenge_name] = true
        switch_challenge(new_challenge)
    end
end

function switch_challenge(challenge_name, state_path)
    state_path = state_path or savestates[challenge_name]

    if state_path and challenge_handlers[challenge_name] then
        client.openrom(challenge_roms[challenge_name])
        savestate.load(state_path)
        current_challenge.name = challenge_name
        current_challenge.handler = challenge_handlers[challenge_name]
        current_challenge.state = {}
    else
        error("Challenge or savestate not found: " .. tostring(challenge_name))
    end
end

math.randomseed(os.time())
switch_challenge("castlevania")
switch_to_random_challenge(current_challenge.name)

local prev_t_state = false

while true do
    if scheduled_switch.active then
        scheduled_switch.frames_left = scheduled_switch.frames_left - 1
        if scheduled_switch.frames_left <= 0 then
            if scheduled_switch.target_challenge then
                switch_challenge(scheduled_switch.target_challenge)
            else
                switch_to_random_challenge(current_challenge.name)
            end
            scheduled_switch.active = false
            scheduled_switch.frames_left = 0
            scheduled_switch.target_challenge = nil
        end
    else
        if current_challenge.handler then
            local current_t_state = input.get()["T"]
            if current_t_state and not prev_t_state then
                switch_to_random_challenge(current_challenge.name, 60)
            end
            prev_t_state = current_t_state
            current_challenge.handler(current_challenge.state)
            local display_name = challenge_names[current_challenge.name] or current_challenge.name
            gui.drawText(10, 10, display_name, "yellow", "black")
            local completed_count = 0
            for _ in pairs(completed_challenges) do
                completed_count = completed_count + 1
            end
            -- gui.drawText(10, 30, string.format("Completed: %d/%d",
            --     completed_count,
            --     #get_challenge_keys()),
            --     "white", "black")
        else
            gui.drawText(10, 10, "No active challenge.", "red")
        end
    end
    emu.frameadvance()
end