#include "nfc_business_card_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const nfc_business_card_on_enter_handlers[])(void*) = {
#include "nfc_business_card_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const nfc_business_card_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "nfc_business_card_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const nfc_business_card_on_exit_handlers[])(void*) = {
#include "nfc_business_card_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers nfc_business_card_scene_handlers = {
    .on_enter_handlers = nfc_business_card_on_enter_handlers,
    .on_event_handlers = nfc_business_card_on_event_handlers,
    .on_exit_handlers = nfc_business_card_on_exit_handlers,
    .scene_num = NfcBusinessCardSceneNum,
};
