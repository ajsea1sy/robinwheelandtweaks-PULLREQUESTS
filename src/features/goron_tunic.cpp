void tint_clothing_material(J3DModelData* model, u16 index) {
    if (model == nullptr || index >= model->getMaterialNum()) {
        return;
    }

    auto* material = model->getMaterialNodePointer(index);
    auto* color = material != nullptr ? material->getTevColor(1) : nullptr;
    if (color == nullptr) {
        return;
    }

    color->r = 72;
    color->g = 0;
    color->b = 0;
}

void tint_goron_armor_indices(J3DModelData* model, const u16* indices,
                              size_t indexCount) {
    if (model == nullptr) {
        return;
    }
    for (size_t i = 0; i < indexCount; ++i) {
        tint_clothing_material(model, indices[i]);
    }
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

bool is_link_clothing_model(const daAlink_c* alink, J3DModelData* model) {
    return alink != nullptr && model != nullptr &&
           (model == alink->field_0x064C ||
            (alink->mpLinkFaceModel != nullptr &&
             model == alink->mpLinkFaceModel->getModelData()) ||
            (alink->mpLinkHatModel != nullptr &&
             model == alink->mpLinkHatModel->getModelData()));
}

void tint_goron_armor_model(daAlink_c* alink, J3DModelData* model) {
    if (!is_link_clothing_model(alink, model)) {
        return;
    }

    tint_clothing_model(model);
    if (model == alink->field_0x064C) {
        static constexpr u16 zoraBodyIndices[] = {13, 0, 1};
        static constexpr u16 magicBodyIndices[] = {11, 10, 9, 8, 6};
        static constexpr u16 casualBodyIndices[] = {7, 5};
        if (alink->checkZoraWearAbility()) {
            tint_goron_armor_indices(model, zoraBodyIndices,
                                     std::size(zoraBodyIndices));
        } else if (alink->checkMagicArmorWearAbility()) {
            tint_goron_armor_indices(model, magicBodyIndices,
                                     std::size(magicBodyIndices));
        } else if (daAlink_c::checkCasualWearFlg()) {
            tint_goron_armor_indices(model, casualBodyIndices,
                                     std::size(casualBodyIndices));
        }
    } else if (alink->mpLinkHatModel != nullptr &&
               model == alink->mpLinkHatModel->getModelData())
    {
        static constexpr u16 zoraHatIndices[] = {1};
        static constexpr u16 magicHatIndices[] = {2, 1};
        static constexpr u16 casualHatIndices[] = {0};
        if (alink->checkZoraWearAbility()) {
            tint_goron_armor_indices(model, zoraHatIndices,
                                     std::size(zoraHatIndices));
        } else if (alink->checkMagicArmorWearAbility()) {
            tint_goron_armor_indices(model, magicHatIndices,
                                     std::size(magicHatIndices));
        } else if (daAlink_c::checkCasualWearFlg()) {
            tint_goron_armor_indices(model, casualHatIndices,
                                     std::size(casualHatIndices));
        }
    }
}

void tint_goron_tunic(ModContext*, void* args, void*, void*) {
    if (!g_goronTunicEquipped) {
        return;
    }

    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr || alink->getClothesChangeWaitTimer() != 0 ||
        alink->field_0x064C == nullptr)
    {
        return;
    }

    tint_goron_armor_model(alink, alink->field_0x064C);
    tint_goron_armor_model(alink, alink->mpLinkFaceModel != nullptr
                                      ? alink->mpLinkFaceModel->getModelData()
                                      : nullptr);
    tint_goron_armor_model(alink, alink->mpLinkHatModel != nullptr
                                      ? alink->mpLinkHatModel->getModelData()
                                      : nullptr);
}

void reapply_tunic_override_after_model_calc(ModContext*, void* args, void*, void*) {
    if (!g_goronTunicEquipped) {
        return;
    }

    auto* alink = mods::arg<daAlink_c*>(args, 0);
    if (alink == nullptr || alink->getClothesChangeWaitTimer() != 0) {
        return;
    }

    auto* model = mods::arg<J3DModel*>(args, 1);
    tint_goron_armor_model(alink, model == nullptr ? nullptr : model->getModelData());
}

void reapply_tunic_override_after_lighting(ModContext*, void* args, void*, void*) {
    if (!g_goronTunicEquipped) {
        return;
    }

    auto* alink = static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
    if (alink == nullptr || alink->getClothesChangeWaitTimer() != 0) {
        return;
    }

    auto* model = mods::arg<J3DModelData*>(args, 1);
    if (!is_link_clothing_model(alink, model)) {
        return;
    }
    tint_goron_armor_model(alink, model);
}
