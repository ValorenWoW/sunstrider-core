-- Patch 2.1
DELETE FROM `game_event_creature` WHERE  `guid`=7426 AND `event`=65;
DELETE FROM `game_event_creature` WHERE  `guid`=78133 AND `event`=65;
DELETE FROM `game_event_creature` WHERE  `guid`=78138 AND `event`=65;
DELETE FROM `game_event_creature` WHERE  `guid`=83719 AND `event`=65;
DELETE FROM `game_event_creature` WHERE  `guid`=84900 AND `event`=65;
DELETE FROM `game_event_creature` WHERE  `guid`=94964 AND `event`=65;
DELETE FROM `game_event_creature` WHERE  `guid`=311075 AND `event`=65;
DELETE FROM `game_event_creature` WHERE  `guid`=311078 AND `event`=65;
DELETE FROM `game_event_gameobject` WHERE  `guid`=17260 AND `event`=65;

UPDATE `creature` SET `patch_min`=12 WHERE `spawnID`=7426;
UPDATE `creature` SET `patch_min`=12 WHERE `spawnID`=78133;
UPDATE `creature` SET `patch_min`=12 WHERE `spawnID`=78138;
UPDATE `creature` SET `patch_min`=12 WHERE `spawnID`=83719;
UPDATE `creature` SET `patch_min`=12 WHERE `spawnID`=84900;
UPDATE `creature` SET `patch_min`=12 WHERE `spawnID`=94964;
UPDATE `creature` SET `patch_min`=12 WHERE `spawnID`=311075;
UPDATE `creature` SET `patch_min`=12 WHERE `spawnID`=311078;

UPDATE `gameobject` SET `patch_min`=12 WHERE `guid`=17260;

UPDATE `quest_template` SET `patch`=12 WHERE `entry`=10955 AND `patch`=0;
UPDATE `quest_template` SET `patch`=12 WHERE `entry`=10969 AND `patch`=0;
UPDATE `quest_template` SET `patch`=12 WHERE `entry`=10971 AND `patch`=0;
UPDATE `quest_template` SET `patch`=12 WHERE `entry`=10973 AND `patch`=0;
UPDATE `quest_template` SET `patch`=12 WHERE `entry`=10984 AND `patch`=0;

-- Patch 2.2
DELETE FROM `game_event_creature` WHERE  `guid`=6715 AND `event`=76;
DELETE FROM `game_event_creature` WHERE  `guid`=68077 AND `event`=76;
DELETE FROM `game_event_creature` WHERE  `guid`=311076 AND `event`=76;
DELETE FROM `game_event_creature` WHERE  `guid`=311077 AND `event`=76;

UPDATE `creature` SET `patch_min`=13 WHERE `spawnID`=6715;
UPDATE `creature` SET `patch_min`=13 WHERE `spawnID`=68077;
UPDATE `creature` SET `patch_min`=13 WHERE `spawnID`=311076;
UPDATE `creature` SET `patch_min`=13 WHERE `spawnID`=311077;

-- Patch 2.3
DELETE FROM `game_event_creature` WHERE  `guid`=10167 AND `event`=66;
DELETE FROM `game_event_creature` WHERE  `guid`=12114 AND `event`=66;
DELETE FROM `game_event_creature` WHERE  `guid`=12223 AND `event`=66;
DELETE FROM `game_event_creature` WHERE  `guid`=12224 AND `event`=66;
DELETE FROM `game_event_creature` WHERE  `guid`=12225 AND `event`=66;
DELETE FROM `game_event_creature` WHERE  `guid`=12226 AND `event`=66;
DELETE FROM `game_event_creature` WHERE  `guid`=17198 AND `event`=66;
DELETE FROM `game_event_creature` WHERE  `guid`=78562 AND `event`=66;
DELETE FROM `game_event_creature` WHERE  `guid`=78564 AND `event`=66;
DELETE FROM `game_event_creature` WHERE  `guid`=80692 AND `event`=66;
DELETE FROM `game_event_creature` WHERE  `guid`=83724 AND `event`=66;
DELETE FROM `game_event_creature` WHERE  `guid`=83725 AND `event`=66;
DELETE FROM `game_event_creature` WHERE  `guid`=83726 AND `event`=66;
DELETE FROM `game_event_creature` WHERE  `guid`=84347 AND `event`=66;
DELETE FROM `game_event_creature` WHERE  `guid`=93104 AND `event`=66;

UPDATE `creature` SET `patch_min`=14 WHERE `spawnID`=10167;
UPDATE `creature` SET `patch_min`=14 WHERE `spawnID`=12114;
UPDATE `creature` SET `patch_min`=14 WHERE `spawnID`=12223;
UPDATE `creature` SET `patch_min`=14 WHERE `spawnID`=12224;
UPDATE `creature` SET `patch_min`=14 WHERE `spawnID`=12225;
UPDATE `creature` SET `patch_min`=14 WHERE `spawnID`=12226;
UPDATE `creature` SET `patch_min`=14 WHERE `spawnID`=17198;
UPDATE `creature` SET `patch_min`=14 WHERE `spawnID`=78562;
UPDATE `creature` SET `patch_min`=14 WHERE `spawnID`=78564;
UPDATE `creature` SET `patch_min`=14 WHERE `spawnID`=80692;
UPDATE `creature` SET `patch_min`=14 WHERE `spawnID`=83724;
UPDATE `creature` SET `patch_min`=14 WHERE `spawnID`=83725;
UPDATE `creature` SET `patch_min`=14 WHERE `spawnID`=83726;
UPDATE `creature` SET `patch_min`=14 WHERE `spawnID`=84347;
UPDATE `creature` SET `patch_min`=14 WHERE `spawnID`=93104;

DELETE FROM `game_event_gameobject` WHERE  `guid`=49870 AND `event`=66;
DELETE FROM `game_event_gameobject` WHERE  `guid`=49871 AND `event`=66;

UPDATE `quest_template` SET `patch`=14 WHERE `entry`=49870 AND `patch`=0;
UPDATE `quest_template` SET `patch`=14 WHERE `entry`=49871 AND `patch`=0;

-- Patch 2.4
DELETE FROM `game_event_creature` WHERE  `guid`=1866 AND `event`=67;
DELETE FROM `game_event_creature` WHERE  `guid`=43249 AND `event`=67;
DELETE FROM `game_event_creature` WHERE  `guid`=64944 AND `event`=67;
DELETE FROM `game_event_creature` WHERE  `guid`=74633 AND `event`=67;
DELETE FROM `game_event_creature` WHERE  `guid`=94969 AND `event`=67;
DELETE FROM `game_event_creature` WHERE  `guid`=95169 AND `event`=67;
DELETE FROM `game_event_creature` WHERE  `guid`=95170 AND `event`=67;
DELETE FROM `game_event_creature` WHERE  `guid`=95171 AND `event`=67;
DELETE FROM `game_event_creature` WHERE  `guid`=95172 AND `event`=67;
DELETE FROM `game_event_creature` WHERE  `guid`=95173 AND `event`=67;
DELETE FROM `game_event_creature` WHERE  `guid`=95174 AND `event`=67;
DELETE FROM `game_event_creature` WHERE  `guid`=95175 AND `event`=67;
DELETE FROM `game_event_creature` WHERE  `guid`=95294 AND `event`=67;
DELETE FROM `game_event_creature` WHERE  `guid`=95362 AND `event`=67;
DELETE FROM `game_event_creature` WHERE  `guid`=311004 AND `event`=67;
DELETE FROM `game_event_creature` WHERE  `guid`=311005 AND `event`=67;

UPDATE `creature` SET `patch_min`=15 WHERE `spawnID`=1866;
UPDATE `creature` SET `patch_min`=15 WHERE `spawnID`=43249;
UPDATE `creature` SET `patch_min`=15 WHERE `spawnID`=74633;
UPDATE `creature` SET `patch_min`=15 WHERE `spawnID`=94969;
UPDATE `creature` SET `patch_min`=15 WHERE `spawnID`=95169;
UPDATE `creature` SET `patch_min`=15 WHERE `spawnID`=95170;
UPDATE `creature` SET `patch_min`=15 WHERE `spawnID`=95171;
UPDATE `creature` SET `patch_min`=15 WHERE `spawnID`=95172;
UPDATE `creature` SET `patch_min`=15 WHERE `spawnID`=95173;
UPDATE `creature` SET `patch_min`=15 WHERE `spawnID`=95174;
UPDATE `creature` SET `patch_min`=15 WHERE `spawnID`=95175;
UPDATE `creature` SET `patch_min`=15 WHERE `spawnID`=95294;
UPDATE `creature` SET `patch_min`=15 WHERE `spawnID`=95362;
UPDATE `creature` SET `patch_min`=15 WHERE `spawnID`=311004;
UPDATE `creature` SET `patch_min`=15 WHERE `spawnID`=311005;
