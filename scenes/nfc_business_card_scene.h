#pragma once

#include <gui/scene_manager.h>

/* Scene enum */
#define ADD_SCENE(prefix, name, id) NfcBusinessCardScene##id,
typedef enum {
#include "nfc_business_card_scene_config.h"
    NfcBusinessCardSceneNum,
} NfcBusinessCardSceneId;
#undef ADD_SCENE

extern const SceneManagerHandlers nfc_business_card_scene_handlers;

/* Handler prototypes */
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void* context);
#include "nfc_business_card_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "nfc_business_card_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "nfc_business_card_scene_config.h"
#undef ADD_SCENE
