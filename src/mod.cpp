#include "mods/service.hpp"
#include "mods/svc/config.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.h"
#include "mods/svc/ui.h"

#include "JSystem/J3DGraphBase/J3DMaterial.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JUtility/JUTNameTab.h"
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "d/d_lib.h"
#include "d/d_meter2_draw.h"
#include "d/d_menu_ring.h"
#include "d/d_menu_window.h"
#include "d/d_meter_map.h"
#include "d/d_meter2_info.h"
#include "d/d_meter_HIO.h"
#include "d/d_pane_class.h"
#include "d/d_select_cursor.h"
#include "m_Do/m_Do_controller_pad.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

DEFINE_MOD();
IMPORT_SERVICE(ConfigService, svc_config);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(UiService, svc_ui);

namespace {

constexpr int kWheelOpensLeft = 0;
constexpr int kWheelOpensDown = 1;
constexpr u8 kGoronTunicSlot = 0xFE;
constexpr u8 kSecondarySlotBase = 0x80;
constexpr u8 kSecondaryWheelSlotCount = MAX_ITEM_SLOTS + 1;
constexpr f32 kSecondaryIconScale = 0.55f;
constexpr f32 kSecondaryCursorScale = 0.5f;
constexpr u8 kGoronTunicItem = dItemNo_WEAR_KOKIRI_e;
constexpr unsigned int kLavaReturnDelayFrames = 5 * 30;

ConfigVarHandle g_wheelOpenDirection = 0;
dMenu_Ring_c* g_secondaryRing = nullptr;
bool g_openingSecondaryRing = false;
bool g_goronTunicEquipped = false;
unsigned int g_lavaSinkFrames = 0;
bool g_primaryRingOpen = false;
bool g_pendingPrimaryItemSync = false;
bool g_deletingSecondaryRing = false;
bool g_tunicInputHandled = false;
int g_keyWaitDepth = 0;
int g_mapInputDepth = 0;
u8 g_savedGroundAtt0 = 0;
u8 g_savedGroundAtt1 = 0;
daAlink_c* g_sandShapeActor = nullptr;

std::array<u8, MAX_ITEM_SLOTS> g_secondarySlots = [] {
    std::array<u8, MAX_ITEM_SLOTS> slots{};
    slots.fill(dItemNo_NONE_e);
    return slots;
}();
std::array<u8, MAX_SELECT_ITEM> g_secondarySelectedItems = [] {
    std::array<u8, MAX_SELECT_ITEM> items{};
    items.fill(dItemNo_NONE_e);
    return items;
}();
std::array<u8, kSecondaryWheelSlotCount> g_secondaryWheelSlotIds{};
std::array<f32, kSecondaryWheelSlotCount> g_secondaryWheelSlotPosX{};
std::array<f32, kSecondaryWheelSlotCount> g_secondaryWheelSlotPosY{};
std::array<s16, kSecondaryWheelSlotCount> g_secondaryWheelSlotAngles{};
std::array<J2DPicture*, 3> g_tunicWheelItemTex{};
std::array<ResTIMG*, 3> g_tunicWheelItemBuf{};
JKRExpHeap* g_tunicWheelItemHeap = nullptr;

bool wheelOpensDown() {
    int64_t value = kWheelOpensDown;
    return g_wheelOpenDirection != 0 &&
           svc_config->get_int(mod_ctx, g_wheelOpenDirection, &value) == MOD_OK &&
           value == kWheelOpensDown;
}

bool mapOpensDown() {
    return !wheelOpensDown();
}

bool isSecondaryRing(const dMenu_Ring_c* ring) {
    return ring != nullptr && ring == g_secondaryRing;
}

bool secondaryTunicAssigned(int button) {
    if (button < 0 || button > 1) {
        return false;
    }

    return g_secondarySelectedItems[button] == kGoronTunicSlot;
}

u8 secondaryItemForSlot(u8 slotId) {
    if (slotId == kGoronTunicSlot) {
        return kGoronTunicItem;
    }

    if (slotId >= kSecondarySlotBase &&
        slotId < kSecondarySlotBase + MAX_ITEM_SLOTS)
    {
        return g_secondarySlots[slotId - kSecondarySlotBase];
    }

    return dItemNo_NONE_e;
}

bool secondaryItemAssigned(int button) {
    if (button < 0 || button > 1) {
        return false;
    }

    return secondaryItemForSlot(g_secondarySelectedItems[button]) != dItemNo_NONE_e;
}

bool is_lava_surface_contact(const daAlink_c* alink) {
    return g_goronTunicEquipped && alink != nullptr && alink->field_0x2fbc == 6 &&
           alink->mWaterY != -G_CM3D_F_INF && alink->mLinkAcch.ChkGroundHit() &&
           alink->current.pos.y <= alink->mWaterY + 1.0f &&
           alink->current.pos.y >= alink->mWaterY - 1.0f;
}

void update_lava_return_timer() {
    auto* alink = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    if (!is_lava_surface_contact(alink) || alink->mSinkShapeOffset >= -0.5f ||
        alink->mProcID == daAlink_c::PROC_LAVA_RETURN)
    {
        g_lavaSinkFrames = 0;
        return;
    }

    if (g_lavaSinkFrames < kLavaReturnDelayFrames) {
        ++g_lavaSinkFrames;
    }
    if (g_lavaSinkFrames == kLavaReturnDelayFrames &&
        alink->procCoLavaReturnInit(FALSE) != 0)
    {
        g_lavaSinkFrames = 0;
        svc_log->info(mod_ctx, "Goron Tunic triggered vanilla lava return");
    }
}

void sync_secondary_select_items() {
    if (g_primaryRingOpen) {
        return;
    }

    for (int button = 0; button < 2; ++button) {
        if (secondaryItemAssigned(button)) {
            g_dComIfG_gameInfo.play.setSelectItem(
                button, secondaryItemForSlot(g_secondarySelectedItems[button]));
        }
    }
}

void tint_tunic_icon(J2DPicture* icon) {
    if (icon != nullptr) {
        icon->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
            JUtility::TColor(190, 24, 24, 255));
    }
}

void tint_tunic_icon(CPaneMgr* icon) {
    if (icon != nullptr) {
        icon->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
            JUtility::TColor(190, 24, 24, 255));
    }
}

void tint_secondary_ring_icons(dMenu_Ring_c* ring) {
    if (!isSecondaryRing(ring)) {
        return;
    }

    for (auto* icon : g_tunicWheelItemTex) {
        tint_tunic_icon(icon);
    }

    for (int button = 0; button < 2; ++button) {
        if (!secondaryTunicAssigned(button)) {
            continue;
        }
        for (auto* icon : ring->mpSelectItemTex[button]) {
            tint_tunic_icon(icon);
        }
    }
}

void clear_tunic_wheel_textures() {
    for (auto*& picture : g_tunicWheelItemTex) {
        if (picture != nullptr) {
            JKR_DELETE(picture);
            picture = nullptr;
        }
    }
    if (g_tunicWheelItemHeap != nullptr) {
        for (auto*& buffer : g_tunicWheelItemBuf) {
            if (buffer != nullptr) {
                g_tunicWheelItemHeap->free(buffer);
                buffer = nullptr;
            }
        }
    }
    g_tunicWheelItemHeap = nullptr;
}

void load_tunic_wheel_textures(dMenu_Ring_c* ring) {
    clear_tunic_wheel_textures();
    g_tunicWheelItemHeap = ring->mpHeap;
    for (auto*& buffer : g_tunicWheelItemBuf) {
        buffer = static_cast<ResTIMG*>(ring->mpHeap->alloc(0xC00, 0x20));
    }

    const int textureCount = dMeter2Info_readItemTexture(
        kGoronTunicItem, g_tunicWheelItemBuf[0], nullptr, g_tunicWheelItemBuf[1], nullptr,
        g_tunicWheelItemBuf[2], nullptr, nullptr, nullptr, -1);
    for (int texture = 0; texture < std::min(textureCount, 3); ++texture) {
        g_tunicWheelItemTex[texture] = JKR_NEW J2DPicture(g_tunicWheelItemBuf[texture]);
        g_tunicWheelItemTex[texture]->setBasePosition(J2DBasePosition_4);
    }
    dMeter2Info_setItemColor(kGoronTunicItem, g_tunicWheelItemTex[0],
        g_tunicWheelItemTex[1], g_tunicWheelItemTex[2], nullptr);
    tint_secondary_ring_icons(ring);
}

void setup_secondary_ring(dMenu_Ring_c* ring) {
    g_secondaryWheelSlotIds.fill(dItemNo_NONE_e);
    for (u8 slot = 0; slot < MAX_ITEM_SLOTS; ++slot) {
        g_secondaryWheelSlotIds[slot] = kSecondarySlotBase + slot;
    }
    g_secondaryWheelSlotIds[MAX_ITEM_SLOTS] = kGoronTunicSlot;

    ring->mItemsTotal = MAX_ITEM_SLOTS;
    ring->mTotalItemTexToAlloc = MAX_ITEM_SLOTS;
    ring->field_0x634 = 0x10000 / kSecondaryWheelSlotCount;
    ring->mCurrentSlot = 0;
    ring->mXButtonSlot = dItemNo_NONE_e;
    ring->mYButtonSlot = dItemNo_NONE_e;
    ring->mStatus = dMenu_Ring_c::STATUS_WAIT;
    ring->mOldStatus = dMenu_Ring_c::STATUS_WAIT;
    ring->field_0x674[0] = 0;
    ring->field_0x674[1] = 0;
    ring->field_0x674[2] = 0;
    ring->field_0x674[3] = 0;
    ring->mRingRadiusH = g_ringHIO.mRingRadiusH;
    ring->mRingRadiusV = g_ringHIO.mRingRadiusV;

    const f32 centerX = g_ringHIO.mItemRingPosX + FB_WIDTH_BASE / 2;
    const f32 centerY = g_ringHIO.mItemRingPosY + FB_HEIGHT_BASE / 2;
    constexpr f32 kTau = 6.28318530717958647692f;
    constexpr int kEllipseSamples = 4096;
    std::array<f32, kEllipseSamples + 1> arcLengths{};
    f32 previousX = 0.0f;
    f32 previousY = -ring->mRingRadiusV;
    for (int sample = 1; sample <= kEllipseSamples; ++sample) {
        const f32 angle = kTau * sample / kEllipseSamples;
        const f32 x = ring->mRingRadiusH * std::sin(angle);
        const f32 y = -ring->mRingRadiusV * std::cos(angle);
        const f32 deltaX = x - previousX;
        const f32 deltaY = y - previousY;
        arcLengths[sample] =
            arcLengths[sample - 1] + std::sqrt(deltaX * deltaX + deltaY * deltaY);
        previousX = x;
        previousY = y;
    }

    const f32 perimeter = arcLengths[kEllipseSamples];
    for (u8 index = 0; index < kSecondaryWheelSlotCount; ++index) {
        const f32 targetDistance = perimeter * index / kSecondaryWheelSlotCount;
        int sample = 1;
        while (sample < kEllipseSamples &&
            arcLengths[sample] < targetDistance)
        {
            ++sample;
        }
        const f32 segmentLength = arcLengths[sample] - arcLengths[sample - 1];
        const f32 segmentFraction = segmentLength > 0.0f
            ? (targetDistance - arcLengths[sample - 1]) / segmentLength
            : 0.0f;
        const f32 angle = kTau * (sample - 1 + segmentFraction) / kEllipseSamples;
        g_secondaryWheelSlotPosX[index] =
            centerX + ring->mRingRadiusH * std::sin(angle);
        g_secondaryWheelSlotPosY[index] =
            centerY - ring->mRingRadiusV * std::cos(angle);
        g_secondaryWheelSlotAngles[index] = cM_atan2s(
            g_secondaryWheelSlotPosX[index] - centerX,
            g_secondaryWheelSlotPosY[index] - centerY);
    }

    for (int i = 0; i < MAX_ITEM_SLOTS; ++i) {
        ring->mItemSlots[i] = dItemNo_NONE_e;
        ring->mItemSlotPosX[i] = g_secondaryWheelSlotPosX[i];
        ring->mItemSlotPosY[i] = g_secondaryWheelSlotPosY[i];
        ring->field_0x63e[i] = g_secondaryWheelSlotAngles[i];
        ring->mItemSlotParam1[i] = 0.0f;
        ring->mItemSlotParam2[i] = 0.0f;
        for (int j = 0; j < 3; ++j) {
            if (ring->mpItemTex[i][j] != nullptr) {
                JKR_DELETE(ring->mpItemTex[i][j]);
                ring->mpItemTex[i][j] = nullptr;
            }
            if (ring->mpItemBuf[i][j] != nullptr) {
                ring->mpHeap->free(ring->mpItemBuf[i][j]);
                ring->mpItemBuf[i][j] = nullptr;
            }
        }
    }

    for (u8 index = 0; index < MAX_ITEM_SLOTS; ++index) {
        const u8 item = secondaryItemForSlot(g_secondaryWheelSlotIds[index]);
        ring->mItemSlots[index] = g_secondaryWheelSlotIds[index];
        if (item == dItemNo_NONE_e) {
            continue;
        }
        for (int texture = 0; texture < 3; ++texture) {
            ring->mpItemBuf[index][texture] =
                static_cast<ResTIMG*>(ring->mpHeap->alloc(0xC00, 0x20));
        }

        const int textureCount = dMeter2Info_readItemTexture(
            item, ring->mpItemBuf[index][0], nullptr, ring->mpItemBuf[index][1], nullptr,
            ring->mpItemBuf[index][2], nullptr, nullptr, nullptr, -1);
        for (int texture = 0; texture < std::min(textureCount, 3); ++texture) {
            ring->mpItemTex[index][texture] = JKR_NEW J2DPicture(ring->mpItemBuf[index][texture]);
            ring->mpItemTex[index][texture]->setBasePosition(J2DBasePosition_4);
        }
        dMeter2Info_setItemColor(item, ring->mpItemTex[index][0], ring->mpItemTex[index][1],
            ring->mpItemTex[index][2], nullptr);
        const f32 scale = dItem_data::getTexScale(item) / 100.0f;
        ring->mItemSlotParam1[index] =
            ring->mpItemBuf[index][0]->width / 48.0f * scale * kSecondaryIconScale;
        ring->mItemSlotParam2[index] =
            ring->mpItemBuf[index][0]->height / 48.0f * scale * kSecondaryIconScale;
    }
    load_tunic_wheel_textures(ring);

    for (int button = 0; button < 2; ++button) {
        const u8 assignedSlot = g_secondarySelectedItems[button];
        ring->setSelectItem(button, secondaryItemForSlot(assignedSlot));
        if (button == 0) {
            ring->mXButtonSlot = dItemNo_NONE_e;
        } else {
            ring->mYButtonSlot = dItemNo_NONE_e;
        }
        for (u8 index = 0; index < kSecondaryWheelSlotCount; ++index) {
            if (g_secondaryWheelSlotIds[index] == assignedSlot) {
                if (button == 0) {
                    ring->mXButtonSlot = index;
                } else {
                    ring->mYButtonSlot = index;
                }
                break;
            }
        }
        if (secondaryTunicAssigned(button)) {
            for (auto* icon : ring->mpSelectItemTex[button]) {
                tint_tunic_icon(icon);
            }
        }
    }
    ring->mpDrawCursor->setPos(
        g_secondaryWheelSlotPosX[0], g_secondaryWheelSlotPosY[0]);
    ring->mpDrawCursor->setParam(
        kSecondaryCursorScale, kSecondaryCursorScale, 0.1f, 0.6f, 0.5f);
    tint_secondary_ring_icons(ring);
}

void set_secondary_ring_name(dMenu_Ring_c* ring) {
    if (ring->mpCenterScreen == nullptr || ring->mCurrentSlot >= kSecondaryWheelSlotCount) {
        return;
    }

    constexpr unsigned long long names[] = {
        MULTI_CHAR('item_n04'), MULTI_CHAR('item_n05'), MULTI_CHAR('item_n06'),
        MULTI_CHAR('item_n07'), MULTI_CHAR('fitem_n1'), MULTI_CHAR('fitem_n2'),
        MULTI_CHAR('fitem_n3'), MULTI_CHAR('fitem_n4')};
    for (const auto nameId : names) {
        if (auto* name = static_cast<J2DTextBox*>(ring->mpCenterScreen->search(nameId))) {
            name->setString(0x40,
                g_secondaryWheelSlotIds[ring->mCurrentSlot] == kGoronTunicSlot
                    ? "Goron Tunic"
                    : "");
        }
    }
}

u8 closest_secondary_wheel_slot(s16 stickAngle) {
    u8 closestSlot = 0;
    int smallestDifference = 0x10000;
    for (u8 index = 0; index < kSecondaryWheelSlotCount; ++index) {
        const s16 difference = static_cast<s16>(
            stickAngle - g_secondaryWheelSlotAngles[index]);
        const int distance = std::abs(static_cast<int>(difference));
        if (distance < smallestDifference) {
            smallestDifference = distance;
            closestSlot = index;
        }
    }
    return closestSlot;
}

void position_secondary_cursor(dMenu_Ring_c* ring);

void move_secondary_ring(dMenu_Ring_c* ring) {
    if (ring->mStatus == dMenu_Ring_c::STATUS_WAIT &&
        ring->mpStick != nullptr &&
        ring->mpStick->getValueStick() >= 0.75f)
    {
        const u8 newSlot = closest_secondary_wheel_slot(ring->mpStick->getAngleStick());
        if (newSlot != ring->mCurrentSlot) {
            ring->mCurrentSlot = newSlot;
            ring->field_0x670 = g_secondaryWheelSlotAngles[newSlot];
        }
    }

    if (ring->mCurrentSlot < kSecondaryWheelSlotCount) {
        const u8 selected = g_secondaryWheelSlotIds[ring->mCurrentSlot];
        if (mDoCPd_c::getTrigX(PAD_1) != 0) {
            g_secondarySelectedItems[0] = selected;
            ring->mXButtonSlot = ring->mCurrentSlot;
            ring->setSelectItem(0, secondaryItemForSlot(selected));
            sync_secondary_select_items();
        }
        if (mDoCPd_c::getTrigY(PAD_1) != 0) {
            g_secondarySelectedItems[1] = selected;
            ring->mYButtonSlot = ring->mCurrentSlot;
            ring->setSelectItem(1, secondaryItemForSlot(selected));
            sync_secondary_select_items();
        }
        for (int button = 0; button < 2; ++button) {
            if (secondaryTunicAssigned(button)) {
                for (auto* icon : ring->mpSelectItemTex[button]) {
                    tint_tunic_icon(icon);
                }
            }
        }
        if (!secondaryTunicAssigned(0) && !secondaryTunicAssigned(1)) {
            g_goronTunicEquipped = false;
        }
    }
    position_secondary_cursor(ring);
    set_secondary_ring_name(ring);
}

void position_secondary_cursor(dMenu_Ring_c* ring) {
    if (ring->mCurrentSlot >= kSecondaryWheelSlotCount) {
        return;
    }

    const int index = ring->mCurrentSlot;
    ring->mpDrawCursor->setPos(g_secondaryWheelSlotPosX[index] + ring->mCenterPosX,
        g_secondaryWheelSlotPosY[index] + ring->mCenterPosY);
    ring->mpDrawCursor->setParam(
        kSecondaryCursorScale, kSecondaryCursorScale, 0.1f, 0.6f, 0.5f);
}

HookAction open_secondary_ring(ModContext*, void* args, void* retval, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!isSecondaryRing(ring)) {
        return HOOK_CONTINUE;
    }

    if (ring->mOpenCloseFrames == 0) {
        dMeter2Info_set2DVibrationM();
    }
    ++ring->mOpenCloseFrames;
    ring->mAlphaRate = static_cast<f32>(ring->mOpenCloseFrames) / g_ringHIO.mOpenFrames;
    if (ring->mRingOrigin == 0) {
        ring->mCenterPosX = 0.0f;
        ring->mCenterPosY = (1.0f - ring->mAlphaRate) * FB_HEIGHT_BASE;
    } else if (ring->mRingOrigin == 2) {
        ring->mCenterPosX = 0.0f;
        ring->mCenterPosY = (1.0f - ring->mAlphaRate) * -FB_HEIGHT_BASE;
    } else if (ring->mRingOrigin == 3) {
        ring->mCenterPosX = (1.0f - ring->mAlphaRate) * FB_WIDTH_BASE;
        ring->mCenterPosY = 0.0f;
    } else {
        ring->mCenterPosX = (1.0f - ring->mAlphaRate) * -FB_WIDTH_BASE;
        ring->mCenterPosY = 0.0f;
    }
    const bool opened = ring->mOpenCloseFrames >= g_ringHIO.mOpenFrames;
    if (opened) {
        ring->mOpenCloseFrames = g_ringHIO.mCloseFrames;
        ring->mAlphaRate = 1.0f;
        ring->mCenterPosX = 0.0f;
        ring->mCenterPosY = 0.0f;
    }
    position_secondary_cursor(ring);
    *static_cast<bool*>(retval) = opened;
    return HOOK_SKIP_ORIGINAL;
}

HookAction close_secondary_ring(ModContext*, void* args, void* retval, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!isSecondaryRing(ring)) {
        return HOOK_CONTINUE;
    }

    if (ring->field_0x674[0] != 0 || ring->field_0x674[1] != 0 ||
        ring->field_0x674[2] != 0 || ring->field_0x674[3] != 0)
    {
        *static_cast<bool*>(retval) = false;
        return HOOK_SKIP_ORIGINAL;
    }

    --ring->mOpenCloseFrames;
    ring->mAlphaRate = static_cast<f32>(ring->mOpenCloseFrames) / g_ringHIO.mCloseFrames;
    const bool closed = ring->mOpenCloseFrames <= 0;
    if (closed) {
        ring->mOpenCloseFrames = 0;
        ring->mAlphaRate = 0.0f;
    }
    if (ring->mRingOrigin == 0) {
        ring->mCenterPosX = 0.0f;
        ring->mCenterPosY = (1.0f - ring->mAlphaRate) * -FB_HEIGHT_BASE;
    } else if (ring->mRingOrigin == 2) {
        ring->mCenterPosX = 0.0f;
        ring->mCenterPosY = (1.0f - ring->mAlphaRate) * FB_HEIGHT_BASE;
    } else if (ring->mRingOrigin == 3) {
        ring->mCenterPosX = (1.0f - ring->mAlphaRate) * -FB_WIDTH_BASE;
        ring->mCenterPosY = 0.0f;
    } else {
        ring->mCenterPosX = (1.0f - ring->mAlphaRate) * FB_WIDTH_BASE;
        ring->mCenterPosY = 0.0f;
    }
    position_secondary_cursor(ring);
    *static_cast<bool*>(retval) = closed;
    return HOOK_SKIP_ORIGINAL;
}

void draw_secondary_ring(dMenu_Ring_c* ring) {
    J2DGrafContext* grafPort = dComIfGp_getCurrentGrafPort();
    grafPort->setup2D();
    if (ring->mRingPosX != g_ringHIO.mRingPosX ||
        ring->mRingPosY != g_ringHIO.mRingPosY)
    {
        ring->mRingPosX = g_ringHIO.mRingPosX;
        ring->mRingPosY = g_ringHIO.mRingPosY;
        ring->mpCircle->paneTrans(ring->mRingPosX, ring->mRingPosY);
    }
    if (ring->mRingScaleH != g_ringHIO.mRingScaleH ||
        ring->mRingScaleV != g_ringHIO.mRingScaleV)
    {
        ring->mRingScaleH = g_ringHIO.mRingScaleH;
        ring->mRingScaleV = g_ringHIO.mRingScaleV;
        ring->mpCircle->scale(ring->mRingScaleH, ring->mRingScaleV);
    }
    ring->mpSpotParent->setAlphaRate(ring->mAlphaRate * g_ringHIO.mOverlayAlpha);
    ring->mpSpotScreen->draw(0.0f, 0.0f, grafPort);
    ring->mpCenterParent->setAlphaRate(ring->mAlphaRate);
    ring->mpCenterScreen->draw(ring->mCenterPosX, ring->mCenterPosY, grafPort);
    ring->mpMessageParent->setAlphaRate(ring->mAlphaRate);
    ring->mpScreen->draw(ring->mCenterPosX, ring->mCenterPosY, grafPort);

    std::array<std::array<J2DPicture*, 3>, MAX_ITEM_SLOTS> savedItemTextures{};
    for (int index = 0; index < MAX_ITEM_SLOTS; ++index) {
        for (int texture = 0; texture < 3; ++texture) {
            savedItemTextures[index][texture] = ring->mpItemTex[index][texture];
            ring->mpItemTex[index][texture] = nullptr;
        }
    }

    ring->drawItem();
    const u8 savedCurrentSlot = ring->mCurrentSlot;
    const f32 savedLastSlotX = ring->mItemSlotPosX[MAX_ITEM_SLOTS - 1];
    const f32 savedLastSlotY = ring->mItemSlotPosY[MAX_ITEM_SLOTS - 1];
    if (ring->mCurrentSlot == MAX_ITEM_SLOTS) {
        ring->mCurrentSlot = MAX_ITEM_SLOTS - 1;
        ring->mItemSlotPosX[MAX_ITEM_SLOTS - 1] =
            g_secondaryWheelSlotPosX[MAX_ITEM_SLOTS];
        ring->mItemSlotPosY[MAX_ITEM_SLOTS - 1] =
            g_secondaryWheelSlotPosY[MAX_ITEM_SLOTS];
    }
    ring->drawItem2();
    ring->mCurrentSlot = savedCurrentSlot;
    ring->mItemSlotPosX[MAX_ITEM_SLOTS - 1] = savedLastSlotX;
    ring->mItemSlotPosY[MAX_ITEM_SLOTS - 1] = savedLastSlotY;
    for (int index = 0; index < MAX_ITEM_SLOTS; ++index) {
        for (int texture = 0; texture < 3; ++texture) {
            ring->mpItemTex[index][texture] = savedItemTextures[index][texture];
        }
    }

    for (u8 index = 0; index < kSecondaryWheelSlotCount; ++index) {
        const f32 x = g_secondaryWheelSlotPosX[index] - 24.0f + ring->mCenterPosX;
        const f32 y = g_secondaryWheelSlotPosY[index] - 24.0f + ring->mCenterPosY;
        if (index == MAX_ITEM_SLOTS) {
            if (g_tunicWheelItemBuf[0] == nullptr) {
                continue;
            }
            const f32 scale =
                dItem_data::getTexScale(kGoronTunicItem) / 100.0f * kSecondaryIconScale;
            const f32 width = g_tunicWheelItemBuf[0]->width / 48.0f * scale * 48.0f;
            const f32 height = g_tunicWheelItemBuf[0]->height / 48.0f * scale * 48.0f;
            for (auto* picture : g_tunicWheelItemTex) {
                if (picture == nullptr) {
                    continue;
                }
                picture->setAlpha(ring->mAlphaRate * 255.0f);
                picture->draw(x + (48.0f - width) * 0.5f,
                    y + (48.0f - height) * 0.5f, width, height, 0, 0, 0);
            }
            continue;
        }

        for (int texture = 0; texture < 3; ++texture) {
            auto* picture = ring->mpItemTex[index][texture];
            if (picture == nullptr) {
                continue;
            }
            picture->setAlpha(ring->mAlphaRate * 255.0f);
            const f32 width = ring->mItemSlotParam1[index] * 48.0f;
            const f32 height = ring->mItemSlotParam2[index] * 48.0f;
            picture->draw(x + (48.0f - width) * 0.5f,
                y + (48.0f - height) * 0.5f, width, height, 0, 0, 0);
        }
    }

    constexpr unsigned long long buttonPanes[] = {
        MULTI_CHAR('x_btn_n'), MULTI_CHAR('y_btn_n')};
    for (int button = 0; button < 2; ++button) {
        if (!secondaryItemAssigned(button)) {
            continue;
        }
        auto* pane = ring->mpScreen->search(buttonPanes[button]);
        if (pane == nullptr) {
            continue;
        }
        CPaneMgr paneMgr;
        const Vec center = paneMgr.getGlobalVtxCenter(pane, true, 0);
        for (int texture = 0; texture < ring->field_0x686[button]; ++texture) {
            auto* picture = ring->mpSelectItemTex[button][texture];
            if (picture == nullptr) {
                continue;
            }
            picture->setAlpha(ring->mAlphaRate * 255.0f);
            const f32 width = ring->field_0x548[button] * 48.0f;
            const f32 height = ring->field_0x558[button] * 48.0f;
            picture->draw(center.x - width * 0.5f + ring->mCenterPosX,
                center.y - height * 0.5f + ring->mCenterPosY,
                width, height, 0, 0, 0);
        }
    }

    ring->mpDrawCursor->draw();
}

HookAction suppress_secondary_explain(ModContext*, void* args, void* retval, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (g_openingSecondaryRing || isSecondaryRing(ring)) {
        *static_cast<u8*>(retval) = 0;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

void name_goron_tunic(ModContext*, void* args, void*, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!isSecondaryRing(ring) || ring->mCurrentSlot >= kSecondaryWheelSlotCount ||
        g_secondaryWheelSlotIds[ring->mCurrentSlot] != kGoronTunicSlot ||
        ring->mpCenterScreen == nullptr)
    {
        return;
    }

    constexpr unsigned long long firstNames[] = {
        MULTI_CHAR('item_n04'), MULTI_CHAR('fitem_n1')};
    constexpr unsigned long long otherNames[] = {
        MULTI_CHAR('fitem_n2'), MULTI_CHAR('fitem_n3'), MULTI_CHAR('fitem_n4')};
    for (const auto nameId : firstNames) {
        if (auto* name = static_cast<J2DTextBox*>(ring->mpCenterScreen->search(nameId))) {
            name->setString(0x40, "Goron Tunic");
        }
    }
    for (const auto nameId : otherNames) {
        if (auto* name = static_cast<J2DTextBox*>(ring->mpCenterScreen->search(nameId))) {
            name->setString(0x40, "Goron Tunic");
        }
    }
}

HookAction begin_key_wait(ModContext*, void*, void*, void*) {
    ++g_keyWaitDepth;
    return HOOK_CONTINUE;
}

void end_key_wait(ModContext*, void*, void*, void*) {
    if (g_keyWaitDepth > 0) {
        --g_keyWaitDepth;
    }
}

HookAction begin_map_input(ModContext*, void*, void*, void*) {
    ++g_mapInputDepth;
    return HOOK_CONTINUE;
}

void end_map_input(ModContext*, void*, void*, void*) {
    if (g_mapInputDepth > 0) {
        --g_mapInputDepth;
    }
}

HookAction remap_down_trigger(ModContext*, void*, void* retval, void*) {
    if (mapOpensDown() &&
        (g_keyWaitDepth > 0 || g_secondaryRing != nullptr))
    {
        *static_cast<BOOL*>(retval) = mDoCPd_c::getTrigLeft(PAD_1) != 0;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction remap_left_trigger(ModContext*, void*, void* retval, void*) {
    if (mapOpensDown() && g_keyWaitDepth > 0) {
        *static_cast<BOOL*>(retval) = false;
        return HOOK_SKIP_ORIGINAL;
    }
    if (mapOpensDown() && g_mapInputDepth > 0) {
        *static_cast<BOOL*>(retval) = mDoCPd_c::getTrigDown(PAD_1) != 0;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction prepare_secondary_wheel(ModContext*, void* args, void*, void*) {
    g_openingSecondaryRing = false;

    const bool secondaryPressed =
        wheelOpensDown() ? mDoCPd_c::getTrigDown(PAD_1) != 0 :
                           mDoCPd_c::getTrigLeft(PAD_1) != 0;
    const bool primaryPressed = mDoCPd_c::getTrigUp(PAD_1) != 0;
    if (g_keyWaitDepth > 0 && secondaryPressed && !primaryPressed) {
        g_openingSecondaryRing = true;
        mods::arg_ref<u8>(args, 1) = mapOpensDown() ? 1 : 2;
    }

    return HOOK_CONTINUE;
}

void finish_secondary_wheel(ModContext*, void* args, void*, void*) {
    auto* window = mods::arg<dMw_c*>(args, 0);
    if (g_openingSecondaryRing) {
        g_secondaryRing = window->mpMenuRing;
        setup_secondary_ring(g_secondaryRing);
    } else {
        g_primaryRingOpen = true;
        g_pendingPrimaryItemSync = true;
    }
    g_openingSecondaryRing = false;
}

HookAction prepare_ring_delete(ModContext*, void* args, void*, void*) {
    auto* window = mods::arg<dMw_c*>(args, 0);
    g_deletingSecondaryRing = isSecondaryRing(window->mpMenuRing);
    if (g_deletingSecondaryRing) {
        clear_tunic_wheel_textures();
    }
    return HOOK_CONTINUE;
}

void clear_secondary_wheel(ModContext*, void*, void*, void*) {
    if (g_deletingSecondaryRing) {
        g_secondaryRing = nullptr;
        g_deletingSecondaryRing = false;
    } else if (g_pendingPrimaryItemSync) {
        dComIfGp_setSelectItem(0);
        dComIfGp_setSelectItem(1);
        g_pendingPrimaryItemSync = false;
        g_primaryRingOpen = false;
    }
}

HookAction handle_secondary_ring_move_end(ModContext*, void* args, void* retval, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!isSecondaryRing(ring)) {
        return HOOK_CONTINUE;
    }

    const bool closing = dMw_UP_TRIGGER() || dMw_DOWN_TRIGGER() || dMw_B_TRIGGER();
    if (closing) {
        if (mDoCPd_c::getTrigLeft(PAD_1) != 0) {
            ring->mRingOrigin = 1;
        } else if (mDoCPd_c::getTrigRight(PAD_1) != 0) {
            ring->mRingOrigin = 3;
        } else if (dMw_DOWN_TRIGGER()) {
            ring->mRingOrigin = 2;
        } else {
            ring->mRingOrigin = 0;
        }
    }
    *static_cast<bool*>(retval) = closing;
    return HOOK_SKIP_ORIGINAL;
}

HookAction handle_secondary_ring_move(ModContext*, void* args, void*, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!isSecondaryRing(ring)) {
        return HOOK_CONTINUE;
    }
    move_secondary_ring(ring);
    return HOOK_SKIP_ORIGINAL;
}

HookAction handle_secondary_ring_draw(ModContext*, void* args, void*, void*) {
    auto* ring = mods::arg<dMenu_Ring_c*>(args, 0);
    if (!isSecondaryRing(ring)) {
        return HOOK_CONTINUE;
    }
    draw_secondary_ring(ring);
    return HOOK_SKIP_ORIGINAL;
}

HookAction prevent_lava_return(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr || !g_goronTunicEquipped) {
        return HOOK_CONTINUE;
    }

    const bool groundedLava =
        !alink->checkMagneBootsOn() && alink->mLinkAcch.ChkGroundHit() &&
        alink->mGndPolyAtt1 == 2;
    if (groundedLava) {
        *static_cast<BOOL*>(retval) = FALSE;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

void treat_lava_as_surface(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!g_goronTunicEquipped || alink == nullptr || alink->field_0x2fbc != 6 ||
        alink->mWaterY == -G_CM3D_F_INF || alink->speed.y > 0.0f ||
        alink->current.pos.y > alink->mWaterY ||
        alink->current.pos.y < alink->mWaterY - alink->mHeight)
    {
        return;
    }

    alink->current.pos.y = alink->mWaterY;
    alink->speed.y = 0.0f;
    alink->mLinkAcch.m_ground_h = alink->mWaterY;
    alink->mLinkAcch.SetGroundHit();
    alink->offModeFlg(daAlink_c::MODE_SWIMMING);
    alink->offNoResetFlg0(daAlink_c::FLG0_UNK_80);
    alink->mGndPolyAtt0 = 3;
    alink->mGndPolyAtt1 = 1;
}

void maintain_lava_ground(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!is_lava_surface_contact(alink)) {
        return;
    }

    alink->mGndPolyAtt1 = 1;
    alink->mGroundCode = 3;
    alink->mLinkAcch.m_ground_h = alink->mWaterY;
}

HookAction land_on_lava_surface(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!is_lava_surface_contact(alink)) {
        return HOOK_CONTINUE;
    }

    const f32 fallDistance = 0.01f * (alink->mFallHeight - alink->current.pos.y);
    alink->procLandInit(fallDistance);
    *static_cast<BOOL*>(retval) = TRUE;
    return HOOK_SKIP_ORIGINAL;
}

HookAction prepare_lava_sinking(ModContext*, void* args, void*, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (!is_lava_surface_contact(alink)) {
        return HOOK_CONTINUE;
    }

    g_sandShapeActor = alink;
    g_savedGroundAtt0 = alink->mGndPolyAtt0;
    g_savedGroundAtt1 = alink->mGndPolyAtt1;
    alink->mGndPolyAtt0 = 3;
    alink->mGndPolyAtt1 = 1;
    return HOOK_CONTINUE;
}

void finish_lava_sinking(ModContext*, void*, void*, void*) {
    if (g_sandShapeActor != nullptr) {
        g_sandShapeActor->mGndPolyAtt0 = g_savedGroundAtt0;
        g_sandShapeActor->mGndPolyAtt1 = g_savedGroundAtt1;
        g_sandShapeActor = nullptr;
    }
}

HookAction suppress_lava_submersion(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (g_goronTunicEquipped && alink != nullptr && alink->field_0x2fbc == 6 &&
        alink->mWaterY != -G_CM3D_F_INF && alink->current.pos.y >= alink->mWaterY - 1.0f)
    {
        *static_cast<bool*>(retval) = false;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction suppress_lava_damage(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (g_goronTunicEquipped && alink != nullptr && alink->field_0x2fbc == 6) {
        *static_cast<int*>(retval) = 0;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

void tint_clothing_material(J3DModelData* model, u16 index) {
    auto* material = model->getMaterialNodePointer(index);
    auto* color = material != nullptr ? material->getTevColor(1) : nullptr;
    if (color == nullptr) {
        return;
    }

    color->r = 72;
    color->g = 0;
    color->b = 0;
}

void tint_clothing_model(J3DModelData* model) {
    if (model == nullptr) {
        return;
    }

    static constexpr const char* tunicMaterials[] = {
        "al_upbody_m", "al_skirt_m", "al_lowbody_m", "ml_body_m", "ml_armor_m",
        "zl_up_armor_m", "al_cap_m", "ml_cap_m", "zl_cap_m"};
    auto* materialNames = model->getMaterialName();
    if (materialNames == nullptr) {
        return;
    }

    for (u16 i = 0; i < model->getMaterialNum(); ++i) {
        const char* name = materialNames->getName(i);
        if (name == nullptr) {
            continue;
        }

        if (std::none_of(std::begin(tunicMaterials), std::end(tunicMaterials),
                [name](const char* tunicMaterial) {
                    return std::strcmp(name, tunicMaterial) == 0;
                }))
        {
            continue;
        }

        tint_clothing_material(model, i);
    }
}

void tint_goron_tunic(ModContext*, void* args, void*, void*) {
    if (!g_goronTunicEquipped) {
        return;
    }

    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr || alink->field_0x064C == nullptr) {
        return;
    }

    tint_clothing_model(alink->field_0x064C);
    tint_clothing_model(alink->mpLinkFaceModel != nullptr
                            ? alink->mpLinkFaceModel->getModelData()
                            : nullptr);
    tint_clothing_model(alink->mpLinkHatModel != nullptr
                            ? alink->mpLinkHatModel->getModelData()
                            : nullptr);
}

void reapply_tunic_override_after_model_calc(ModContext*, void* args, void*, void*) {
    if (!g_goronTunicEquipped) {
        return;
    }

    auto* model = mods::arg<J3DModel*>(args, 1);
    tint_clothing_model(model != nullptr ? model->getModelData() : nullptr);
}

HookAction sync_secondary_item_hook(ModContext*, void* args, void*, void*) {
    if (g_primaryRingOpen) {
        return HOOK_CONTINUE;
    }

    const int button = mods::arg<int>(args, 0);
    if (button >= 0 && button < 2 && secondaryItemAssigned(button)) {
        g_dComIfG_gameInfo.play.setSelectItem(
            button, secondaryItemForSlot(g_secondarySelectedItems[button]));
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction handle_tunic_item_trigger(ModContext*, void* args, void* retval, void*) {
    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr || g_secondaryRing != nullptr || g_primaryRingOpen) {
        return HOOK_CONTINUE;
    }

    const u8 buttonFlag = mods::arg<u8>(args, 1);
    const bool xPressed = (buttonFlag & daAlink_c::BTN_X) != 0 &&
                          mDoCPd_c::getTrigX(PAD_1) != 0 && secondaryTunicAssigned(0);
    const bool yPressed = (buttonFlag & daAlink_c::BTN_Y) != 0 &&
                          mDoCPd_c::getTrigY(PAD_1) != 0 && secondaryTunicAssigned(1);
    if (xPressed || yPressed) {
        if (!g_tunicInputHandled) {
            g_goronTunicEquipped = !g_goronTunicEquipped;
            g_lavaSinkFrames = 0;
            g_tunicInputHandled = true;
            svc_log->info(mod_ctx, g_goronTunicEquipped ? "Goron Tunic equipped" :
                                                         "Goron Tunic unequipped");
        }
        *static_cast<BOOL*>(retval) = FALSE;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

void tint_goron_tunic_hud_icon(ModContext*, void* args, void*, void*) {
    const int button = mods::arg<int>(args, 1);
    const u8 item = mods::arg<u8>(args, 2);
    if (!secondaryTunicAssigned(button) || item != kGoronTunicItem) {
        return;
    }

    auto* meter = mods::arg<dMeter2Draw_c*>(args, 0);
    tint_tunic_icon(meter->mpItemXY[button]);
    tint_tunic_icon(meter->mpItemXYPane[button]);
}

ModResult build_panel(ModContext*, UiElementHandle panel, void*, ModError*) {
    static const char* kWheelDirectionOptions[] = {"Left", "Down"};
    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_DROPDOWN;
    control.label = "D-pad direction that opens the secondary inventory wheel";
    control.help_rml =
        "The other Left/Down direction opens the map. In the secondary wheel, press X or Y on "
        "the Goron Tunic to equip or unequip it. The tunic color override is applied after "
        "game-level model color updates; integration with Crests' remote-player color "
        "replication is experimental.";
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = g_wheelOpenDirection;
    control.options = kWheelDirectionOptions;
    control.option_count = 2;
    return svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr);
}

#define ADD_PRE(HookType, callback)                                                                \
    do {                                                                                           \
        const ModResult hookResult = mods::hook::add_pre<HookType>(callback);                      \
        if (hookResult != MOD_OK) {                                                                \
            svc_log->error(mod_ctx, "failed to install secondary wheel hook");                   \
            return hookResult;                                                                     \
        }                                                                                          \
    } while (false)

#define ADD_POST(HookType, callback)                                                               \
    do {                                                                                           \
        const ModResult hookResult = mods::hook::add_post<HookType>(callback);                     \
        if (hookResult != MOD_OK) {                                                                \
            svc_log->error(mod_ctx, "failed to install secondary wheel hook");                   \
            return hookResult;                                                                     \
        }                                                                                          \
    } while (false)

DEFINE_HOOK(&dMenu_Ring_c::openExplain, OpenExplain);
DEFINE_HOOK(&dMenu_Ring_c::setNameString, SetNameString);
DEFINE_HOOK(&dMeter2Draw_c::changeTextureItemXY, ChangeTextureItemXY);
DEFINE_HOOK(&daAlink_c::setWaterY, SetWaterY);
DEFINE_HOOK(&daAlink_c::setRoomInfo, SetRoomInfo);
DEFINE_HOOK(&daAlink_c::checkLandAction, CheckLandAction);
DEFINE_HOOK(&daAlink_c::setSandShapeOffset, SetSandShapeOffset);
DEFINE_HOOK(&daAlink_c::checkWaterPolygonUnder, CheckWaterPolygonUnder);
DEFINE_HOOK(&daAlink_c::checkPolyDamage, CheckPolyDamage);
DEFINE_HOOK(&daAlink_c::itemTriggerCheck, ItemTriggerCheck);
DEFINE_HOOK(&dComIfGp_setSelectItem, SetSelectItem);
DEFINE_HOOK(&dMenu_Ring_c::_move, RingMove);
DEFINE_HOOK(&dMenu_Ring_c::_draw, RingDraw);
DEFINE_HOOK(&dMenu_Ring_c::isOpen, RingIsOpen);
DEFINE_HOOK(&dMenu_Ring_c::isMoveEnd, RingIsMoveEnd);
DEFINE_HOOK(&dMenu_Ring_c::isClose, RingIsClose);
DEFINE_HOOK(&dMw_c::key_wait_proc, KeyWait);
DEFINE_HOOK(&dMw_c::dMw_ring_create, RingCreate);
DEFINE_HOOK(&dMw_c::dMw_ring_delete, RingDelete);
DEFINE_HOOK(&dMw_DOWN_TRIGGER, DownTrigger);
DEFINE_HOOK(&dMw_LEFT_TRIGGER, LeftTrigger);
DEFINE_HOOK(&dMeterMap_c::meter_map_move, MapMeterMove);
DEFINE_HOOK(&dMeterMap_c::keyCheck, MapKeyCheck);
DEFINE_HOOK(&dMeterMap_c::ctrlShowMap, MapShow);
DEFINE_HOOK(&daAlink_c::checkRestartRoom, CheckRestartRoom);
DEFINE_HOOK(&daAlink_c::modelCalc, ModelCalc);
DEFINE_HOOK(&daAlink_c::setWaterDropColor, SetWaterDropColor);

ModResult install_hooks() {
    ADD_PRE(OpenExplain, suppress_secondary_explain);
    ADD_POST(SetNameString, name_goron_tunic);
    ADD_POST(ChangeTextureItemXY, tint_goron_tunic_hud_icon);
    ADD_POST(SetWaterY, treat_lava_as_surface);
    ADD_POST(SetRoomInfo, maintain_lava_ground);
    ADD_PRE(CheckLandAction, land_on_lava_surface);
    ADD_PRE(SetSandShapeOffset, prepare_lava_sinking);
    ADD_POST(SetSandShapeOffset, finish_lava_sinking);
    ADD_PRE(CheckWaterPolygonUnder, suppress_lava_submersion);
    ADD_PRE(CheckPolyDamage, suppress_lava_damage);
    ADD_PRE(ItemTriggerCheck, handle_tunic_item_trigger);
    ADD_PRE(SetSelectItem, sync_secondary_item_hook);
    ADD_PRE(RingMove, handle_secondary_ring_move);
    ADD_PRE(RingDraw, handle_secondary_ring_draw);
    ADD_PRE(RingIsMoveEnd, handle_secondary_ring_move_end);
    ADD_PRE(RingIsOpen, open_secondary_ring);
    ADD_PRE(RingIsClose, close_secondary_ring);

    ADD_PRE(KeyWait, begin_key_wait);
    ADD_POST(KeyWait, end_key_wait);
    ADD_PRE(RingCreate, prepare_secondary_wheel);
    ADD_POST(RingCreate, finish_secondary_wheel);
    ADD_PRE(RingDelete, prepare_ring_delete);
    ADD_POST(RingDelete, clear_secondary_wheel);
    ADD_PRE(DownTrigger, remap_down_trigger);
    ADD_PRE(LeftTrigger, remap_left_trigger);

    ADD_PRE(MapMeterMove, begin_map_input);
    ADD_POST(MapMeterMove, end_map_input);
    ADD_PRE(MapKeyCheck, begin_map_input);
    ADD_POST(MapKeyCheck, end_map_input);
    ADD_PRE(MapShow, begin_map_input);
    ADD_POST(MapShow, end_map_input);
    ADD_PRE(CheckRestartRoom, prevent_lava_return);
    ADD_POST(ModelCalc, reapply_tunic_override_after_model_calc);
    ADD_POST(SetWaterDropColor, tint_goron_tunic);
    return MOD_OK;
}

#undef ADD_PRE
#undef ADD_POST

}  // namespace

extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError* error) {
    ConfigVarDesc config = CONFIG_VAR_DESC_INIT;
    config.name = "mapOpenDirection";
    config.type = CONFIG_VAR_INT;
    config.default_int = kWheelOpensDown;
    ModResult result = svc_config->register_var(mod_ctx, &config, &g_wheelOpenDirection);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to register secondary wheel direction setting");
        return result;
    }

    UiModsPanelDesc panel = UI_MODS_PANEL_DESC_INIT;
    panel.build = build_panel;
    result = svc_ui->register_mods_panel(mod_ctx, &panel);
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "failed to register the secondary wheel settings panel");
        return result;
    }

    result = install_hooks();
    if (result != MOD_OK) {
        return result;
    }
    svc_log->info(mod_ctx, "secondary inventory wheel initialized");
    (void)error;
    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    if (mDoCPd_c::getTrigX(PAD_1) == 0 && mDoCPd_c::getTrigY(PAD_1) == 0) {
        g_tunicInputHandled = false;
    }
    update_lava_return_timer();
    sync_secondary_select_items();
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    g_secondaryRing = nullptr;
    g_openingSecondaryRing = false;
    g_goronTunicEquipped = false;
    g_lavaSinkFrames = 0;
    g_primaryRingOpen = false;
    g_pendingPrimaryItemSync = false;
    g_deletingSecondaryRing = false;
    g_tunicInputHandled = false;
    g_sandShapeActor = nullptr;
    clear_tunic_wheel_textures();
    g_keyWaitDepth = 0;
    g_mapInputDepth = 0;
    svc_log->info(mod_ctx, "secondary inventory wheel unloaded");
    return MOD_OK;
}
}
