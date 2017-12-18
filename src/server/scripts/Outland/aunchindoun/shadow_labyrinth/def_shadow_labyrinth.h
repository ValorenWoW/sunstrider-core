
#ifndef DEF_SHADOW_LABYRINTH_H
#define DEF_SHADOW_LABYRINTH_H

#define SLScriptName "instance_shadow_labyrinth"
#define DataHeader "SL"

uint32 const EncounterCount = 4;

enum SLDataTypes
{
    // Encounter States/Boss GUIDs
    DATA_AMBASSADOR_HELLMAW             = 0,
    DATA_BLACKHEART_THE_INCITER         = 1,
    DATA_GRANDMASTER_VORPIL             = 2,
    DATA_MURMUR                         = 3,

    // Additional Data
    DATA_FEL_OVERSEER                   = 4
};

enum SLCreatureIds
{
    NPC_AMBASSADOR_HELLMAW              = 18731,
    NPC_BLACKHEART                      = 18667,
    NPC_BLACKHEART_DUMMY1               = 19300,
    NPC_BLACKHEART_DUMMY2               = 19301,
    NPC_BLACKHEART_DUMMY3               = 19302,
    NPC_BLACKHEART_DUMMY4               = 19303,
    NPC_BLACKHEART_DUMMY5               = 19304,
    NPC_GRANDMASTER_VORPIL              = 18732,
    NPC_FEL_OVERSEER                    = 18796
};

enum SLGameObjectIds
{
    GO_REFECTORY_DOOR                   = 183296, // door opened when blackheart the inciter dies
    GO_SCREAMING_HALL_DOOR              = 183295  // door opened when grandmaster vorpil dies
};

enum SLMisc
{
    ACTION_AMBASSADOR_HELLMAW_INTRO = 1,
    ACTION_AMBASSADOR_HELLMAW_BANISH = 2,
};

template <class AI, class T>
inline AI* GetShadowLabyrinthAI(T* obj)
{
    return GetInstanceAI<AI>(obj, SLScriptName);
}

#endif

