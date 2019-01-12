-- Update boss armor

-- Warriors
-- 7700 = 6792 * 1,133686
 UPDATE creature_template SET ArmorModifier = 1.133686 WHERE entry IN (
    18831, -- HKM
    17533, -- Romulo (Karazhan)
    17543, -- Strawman (Karazhan)
    17521, -- Big Bad Wolf (Karazhan)
    19044, -- Gruul
    17257, -- Magtheridon
    23576, -- Nalorakk
    23578, -- Jan'alai
    23577, -- Halazzi
    23863, -- Zul'jin
    21216, -- Hydross
    21217, -- Lurker
    21215, -- Leotheras
    21213, -- Morogrim
    19514, -- Al'ar
    22887, -- Naj'entus
    22898, -- Supremus
    22841, -- Akama
    22948, -- Gurtogg
    23419, -- RoS 2
    23420, -- RoS 3
    22917, -- Illidan
    24882, -- Brutallus
    25741, -- M'uru
    25840, -- Entropius
    17225, -- Nightbane
    15690 -- Prince Malchezaar
    
); 

-- Paladins
-- 7700 = 5474 * 1.406649
UPDATE creature_template SET ArmorModifier = 1.406649 WHERE entry = 23574; -- Akil'zon
-- 6200 = 5474 * 1.1326269
UPDATE creature_template SET ArmorModifier = 1.1326269 WHERE entry IN (
    24239, -- Malacrass
    21214, -- Karathress
    21212, -- Vashj
    18805, -- Solarian
    19622, -- KT
    17767, -- Rage Winterchill
    17808, -- Anetheron
    17888, -- Kaz'rogal
    17842, -- Azgalor
    17968, -- Archimonde
    22871, -- Teron
    22947, -- Shahraz
    22949, -- Gathios (Council)
    24850, -- Kalecgos
    24892, -- Sathrovarr
    25038, -- Felmyst
    25166, -- Alythess
    25165, -- Sacrolash
    15691, -- The Curator 
    21966, -- Fathom-Guard Sharkkis
    21965, -- Fathom-Guard Tidalvess
    21964, -- Fathom-Guard Caribdis
    22950, -- High Nethermancer Zerevor (Council)
    22951, -- Lady Malande (Council)
    25315, -- Kil'Jaeden
    18835, -- Kiggler the Crazed (Gruul's Lair HKM friend)
    18836, -- Blindeye the Seer (Gruul's Lair)
    18834, -- Olm the Summoner (Gruul's Lair)
    18832, -- Krosh Firehand (Gruul's Lair)
    17535, -- Dorothee (Karazhan)
    17534, -- Julianne (Karazhan)
    18168 -- The Crone (Karazhan)
);
-- 8800 = 5474 * 1.607599
UPDATE creature_template SET ArmorModifier = 1.607599 WHERE entry = 19516; -- Void Reaver

--  200 = 5474 * 0.036536
UPDATE creature_template SET ArmorModifier = 0.036536 WHERE entry = 22952; -- Veras Darkshadow (Council)
-- 10700= 6792 * 1.575382  
UPDATE creature_template SET ArmorModifier = 1.575382 WHERE entry = 17547; -- Tinhead (Karazhan)
-- 4700 = 6792 * 0.691990
UPDATE creature_template SET ArmorModifier = 0.691990 WHERE entry = 17546; -- Roar (Karazhan)
UPDATE creature_template SET ArmorModifier = 0 WHERE entry = 23418; -- RoS 1
UPDATE creature_template SET ArmorModifier = 0.9975498 WHERE entry = 25799; -- Shadowsword Fury Mage
UPDATE creature_template SET ArmorModifier = 1.001551 WHERE entry = 25798; -- Shadowsword Fury Mage
UPDATE creature_template SET ArmorModifier = 0.959232 WHERE entry = 25772; -- Void Sentinel
UPDATE creature_template SET ArmorModifier = 1.207302 WHERE entry = 16151; -- Midnight (Karazhan)
UPDATE creature_template SET ArmorModifier = 1.104240 WHERE entry = 16151; -- Attumen the Huntsman (Karazhan)
UPDATE creature_template SET ArmorModifier = 1.077822 WHERE entry = 25588; -- Hand of the Deceiver