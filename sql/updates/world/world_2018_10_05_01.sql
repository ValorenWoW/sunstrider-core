
-- Add `position_o` column to spawn in right orientation on toon creation
ALTER TABLE `playercreateinfo` ADD `position_o` FLOAT NOT NULL AFTER `position_z`;

-- Fill `position_o` column with data
UPDATE `playercreateinfo` SET `position_o` = 6.02937 WHERE `race` = 1;  -- Human
UPDATE `playercreateinfo` SET `position_o` = 6.00392 WHERE `race` = 2;  -- Orc
UPDATE `playercreateinfo` SET `position_o` = 6.23886 WHERE `race` = 3;  -- Dwarf
UPDATE `playercreateinfo` SET `position_o` = 6.02401 WHERE `race` = 4;  -- Nightelf
UPDATE `playercreateinfo` SET `position_o` = 3.07090 WHERE `race` = 5;  -- Undead
UPDATE `playercreateinfo` SET `position_o` = 3.08639 WHERE `race` = 6;  -- Tauren
UPDATE `playercreateinfo` SET `position_o` = 6.23886 WHERE `race` = 7;  -- Gnome
UPDATE `playercreateinfo` SET `position_o` = 6.00392 WHERE `race` = 8;  -- Troll
UPDATE `playercreateinfo` SET `position_o` = 5.22683 WHERE `race` = 10; -- Bloodelf
UPDATE `playercreateinfo` SET `position_o` = 6.04596 WHERE `race` = 11; -- Draenei
