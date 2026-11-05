u32 sit_music_playback_id(Z2SeqMgr* sequenceManager, u32) {
    return sequenceManager->getMainBgmID();
}

const SitMusicWaveMapping* sit_music_wave_mapping(u32 track) {
    for (const auto& mapping : kSitMusicWaveMappings) {
        if (mapping.track == track) {
            return &mapping;
        }
    }
    return nullptr;
}

bool sit_music_mapping_uses_wave(const SitMusicWaveMapping* mapping, u32 wave) {
    return mapping != nullptr &&
        (mapping->wave1 == wave || mapping->alternateWave1 == wave ||
         mapping->wave2 == wave);
}

void report_sit_music_wave_failure(u32 track, u32 wave, const char* action) {
    if (g_sitMusicWaveFailureTrack == track) {
        return;
    }

    char message[128];
    std::snprintf(message, sizeof(message),
                  "Sit music wave %02x for track %08x could not %s",
                  wave, track, action);
    svc_log->warn(mod_ctx, message);
    g_sitMusicWaveFailureTrack = track;
}

void set_sit_music_battle_override(Z2SeqMgr* sequenceManager) {
    if (!g_sitMusicBattleOverrideActive) {
        g_sitMusicOriginalBattleBgmOff =
            sequenceManager->mFlags.mBattleBgmOff;
        g_sitMusicBattleOverrideActive = true;
    }
    sequenceManager->setBattleBgmOff(true);
    sequenceManager->mBattleSeqCount = 0;
}

bool load_sit_music_wave(Z2SceneMgr* sceneManager, u8 wave) {
    if (sceneManager->loadBgmWave(wave)) {
        return true;
    }

    if (sceneManager->getBgmLoadStatus(wave) != 0) {
        return false;
    }

    auto* sectionHeap = JAUSectionHeap::getInstance();
    auto* waveBank = sectionHeap != nullptr
        ? sectionHeap->getWaveBankTable().getWaveBank(1) : nullptr;
    auto* waveArc = waveBank != nullptr ? waveBank->getWaveArc(wave) : nullptr;
    return waveArc != nullptr && waveArc->load(nullptr);
}

bool prepare_sit_music_scene_waves(const SitMusicWaveMapping* mapping) {
    auto* audioManager = Z2GetAudioMgr();
    auto* sequenceManager = audioManager != nullptr
        ? static_cast<Z2SeqMgr*>(audioManager) : nullptr;
    auto* sceneManager = audioManager != nullptr
        ? static_cast<Z2SceneMgr*>(audioManager) : nullptr;
    if (sequenceManager == nullptr || sceneManager == nullptr) {
        return false;
    }

    if (!g_sitMusicWaveSnapshot.active) {
        g_sitMusicWaveSnapshot.active = true;
        g_sitMusicWaveSnapshot.requestWave1 = sceneManager->requestBgmWave_1;
        g_sitMusicWaveSnapshot.requestWave2 = sceneManager->requestBgmWave_2;
        g_sitMusicWaveSnapshot.loadedWave1 = sceneManager->loadedBgmWave_1;
        g_sitMusicWaveSnapshot.loadedWave2 = sceneManager->loadedBgmWave_2;
        g_sitMusicWaveSnapshot.mainTrack = sequenceManager->getMainBgmID();
        g_sitMusicWaveSnapshot.mainStatus = sequenceManager->mBgmStatus;
        set_sit_music_battle_override(sequenceManager);
        if (sequenceManager->mMainBgmHandle) {
            sequenceManager->mMainBgmHandle->stop(0);
            sequenceManager->mMainBgmHandle.releaseSound();
        }
        sequenceManager->mMainBgmMaster.forceOut();
    }

    sequenceManager->mFlags.mBattleBgmOff = true;
    sequenceManager->mBattleSeqCount = 0;
    if (g_sitMusicWaveTrack == mapping->track) {
        return true;
    }

    if (mapping->wave1 == 0 && mapping->wave2 == 0) {
        g_sitMusicWaveTrack = mapping->track;
        return true;
    }

    auto* sectionHeap = JAUSectionHeap::getInstance();
    auto* waveBank = sectionHeap != nullptr
        ? sectionHeap->getWaveBankTable().getWaveBank(1) : nullptr;
    if (waveBank == nullptr) {
        return false;
    }

    const u8 sceneWaves[] = {
        g_sitMusicWaveSnapshot.loadedWave1,
        g_sitMusicWaveSnapshot.loadedWave2,
    };
    for (u8 wave : sceneWaves) {
        if (wave == 0 || sit_music_mapping_uses_wave(mapping, wave)) {
            continue;
        }
        const s32 status = sceneManager->getBgmLoadStatus(wave);
        if (status == 1) {
            return false;
        }
        if (status == 2 && !sceneManager->eraseBgmWave(wave)) {
            report_sit_music_wave_failure(mapping->track, wave, "release");
            return false;
        }
        if (sceneManager->loadedBgmWave_1 == wave) {
            sceneManager->loadedBgmWave_1 = 0;
        }
        if (sceneManager->loadedBgmWave_2 == wave) {
            sceneManager->loadedBgmWave_2 = 0;
        }
    }
    sceneManager->requestBgmWave_1 = 0;
    sceneManager->requestBgmWave_2 = 0;
    g_sitMusicWaveTrack = mapping->track;
    return true;
}

bool prepare_sit_music_waves(u32 track) {
    const SitMusicWaveMapping* mapping = sit_music_wave_mapping(track);
    if (mapping == nullptr) {
        return true;
    }

    if (g_sitMusicWaveFailureTrack == track) {
        return false;
    }

    if (!prepare_sit_music_scene_waves(mapping)) {
        return false;
    }

    auto* audioManager = Z2GetAudioMgr();
    auto* sceneManager = audioManager != nullptr
        ? static_cast<Z2SceneMgr*>(audioManager) : nullptr;
    if (sceneManager == nullptr) {
        return false;
    }
    if (mapping->wave1 == 0 && mapping->wave2 == 0) {
        return true;
    }
    u8 wave1 = g_sitMusicResolvedWave1 != 0
        ? g_sitMusicResolvedWave1 : mapping->wave1;
    const u8 wave2 = mapping->wave2;
    const u8 waves[] = {wave1, wave2};
    for (u8 wave : waves) {
        if (wave == 0) {
            continue;
        }

        s32 status = sceneManager->getBgmLoadStatus(wave);
        if (status == 2) {
            if (wave == wave1) {
                g_sitMusicResolvedWave1 = wave1;
            }
            g_sitMusicWaveReleaseFrames[wave] = 0;
            g_sitMusicWaveReleaseScheduled[wave] = false;
            continue;
        }
        if (status == 1) {
            return false;
        }

        if (!load_sit_music_wave(sceneManager, wave)) {
            const s32 loadStatus = sceneManager->getBgmLoadStatus(wave);
            if (loadStatus == 1) {
                g_sitMusicOwnedWaves[wave] = true;
                if (wave == wave1) {
                    g_sitMusicResolvedWave1 = wave1;
                }
                return false;
            }
            if (loadStatus == 2) {
                if (wave == wave1) {
                    g_sitMusicResolvedWave1 = wave1;
                }
                continue;
            }
            if (wave == wave1 && mapping->alternateWave1 != 0 &&
                wave != mapping->alternateWave1)
            {
                wave1 = mapping->alternateWave1;
                g_sitMusicResolvedWave1 = wave1;
                status = sceneManager->getBgmLoadStatus(wave1);
                if (status == 2) {
                    continue;
                }
                if (status == 1) {
                    return false;
                }
                if (!load_sit_music_wave(sceneManager, wave1)) {
                    const s32 fallbackStatus =
                        sceneManager->getBgmLoadStatus(wave1);
                    if (fallbackStatus == 1) {
                        g_sitMusicOwnedWaves[wave1] = true;
                        return false;
                    }
                    if (fallbackStatus == 2) {
                        continue;
                    }
                    report_sit_music_wave_failure(track, wave1, "load");
                    return false;
                }
                g_sitMusicOwnedWaves[wave1] = true;
                return false;
            }
            report_sit_music_wave_failure(track, wave, "load");
            return false;
        }
        g_sitMusicOwnedWaves[wave] = true;
        if (wave == wave1) {
            g_sitMusicResolvedWave1 = wave1;
        }
        g_sitMusicWaveReleaseFrames[wave] = 0;
        g_sitMusicWaveReleaseScheduled[wave] = false;
        return false;
    }

    sceneManager->requestBgmWave_1 = wave1;
    sceneManager->loadedBgmWave_1 = wave1;
    sceneManager->requestBgmWave_2 = mapping->wave2;
    sceneManager->loadedBgmWave_2 = mapping->wave2;
    return true;
}

void schedule_sit_music_wave_releases(u32 activeTrack, u8 delayFrames) {
    const SitMusicWaveMapping* mapping = sit_music_wave_mapping(activeTrack);
    for (u32 wave = 1; wave < g_sitMusicOwnedWaves.size(); ++wave) {
        if (g_sitMusicOwnedWaves[wave] &&
            !sit_music_mapping_uses_wave(mapping, wave))
        {
            g_sitMusicWaveReleaseFrames[wave] = delayFrames;
            g_sitMusicWaveReleaseScheduled[wave] = true;
        }
    }
}

void update_sit_music_wave_cleanup() {
    if (g_sitMusicWaveSnapshot.active) {
        return;
    }

    auto* audioManager = Z2GetAudioMgr();
    auto* sceneManager = audioManager != nullptr
        ? static_cast<Z2SceneMgr*>(audioManager) : nullptr;
    if (sceneManager == nullptr) {
        return;
    }

    const int64_t selection = g_sitActionState == SitActionState::Seated
        ? sitMusicSelection()
        : -1;
    const u32 selectedTrack = selection == 0 ? kSitMusicBgmId : 0;
    const u32 waveTrack = selectedTrack;
    const SitMusicWaveMapping* selectedMapping =
        g_sitMusicReportedFailureTrack == selectedTrack
            ? nullptr : sit_music_wave_mapping(waveTrack);

    for (u32 wave = 1; wave < g_sitMusicOwnedWaves.size(); ++wave) {
        if (!g_sitMusicOwnedWaves[wave]) {
            continue;
        }
        if (sit_music_mapping_uses_wave(selectedMapping, wave)) {
            g_sitMusicWaveReleaseFrames[wave] = 0;
            g_sitMusicWaveReleaseScheduled[wave] = false;
            continue;
        }
        if (wave == sceneManager->loadedBgmWave_1 ||
            wave == sceneManager->loadedBgmWave_2)
        {
            g_sitMusicOwnedWaves[wave] = false;
            g_sitMusicWaveReleaseFrames[wave] = 0;
            g_sitMusicWaveReleaseScheduled[wave] = false;
            continue;
        }
        if (!g_sitMusicWaveReleaseScheduled[wave]) {
            g_sitMusicWaveReleaseFrames[wave] = 30;
            g_sitMusicWaveReleaseScheduled[wave] = true;
        }
        if (g_sitMusicWaveReleaseFrames[wave] > 0) {
            --g_sitMusicWaveReleaseFrames[wave];
            continue;
        }

        const s32 status = sceneManager->getBgmLoadStatus(wave);
        if (status == 0) {
            g_sitMusicOwnedWaves[wave] = false;
            g_sitMusicWaveReleaseScheduled[wave] = false;
            continue;
        }
        if (status == 1) {
            continue;
        }
        if (!sceneManager->eraseBgmWave(wave)) {
            if (!g_sitMusicWaveEraseFailureReported[wave]) {
                char message[96];
                std::snprintf(message, sizeof(message),
                              "Failed to release Sit music wave %02x", wave);
                svc_log->warn(mod_ctx, message);
                g_sitMusicWaveEraseFailureReported[wave] = true;
            }
            continue;
        }
        g_sitMusicOwnedWaves[wave] = false;
        g_sitMusicWaveEraseFailureReported[wave] = false;
        g_sitMusicWaveReleaseScheduled[wave] = false;
    }
}

void log_sit_music_start_failure(u32 selectedTrack, u32 activeTrack);

bool start_sit_music_track(u32 selectedTrack) {
    auto* audioManager = Z2GetAudioMgr();
    auto* sequenceManager = audioManager != nullptr
        ? static_cast<Z2SeqMgr*>(audioManager) : nullptr;
    if (sequenceManager == nullptr) {
        return false;
    }

    const u32 track = selectedTrack;
    if (!prepare_sit_music_waves(track)) {
        return false;
    }
    set_sit_music_battle_override(sequenceManager);

    if (sequenceManager->mFanfareID == track) {
        sequenceManager->mFanfareID = static_cast<JAISoundID>(-1);
    }
    if (sequenceManager->mFanfareHandle &&
        sequenceManager->mFanfareHandle->getID() == track)
    {
        sequenceManager->mFanfareHandle->stop(0);
        sequenceManager->mFanfareHandle.releaseSound();
        sequenceManager->mFanfareCount = 0;
    }
    if (sequenceManager->mSubBgmHandle)
    {
        sequenceManager->mSubBgmHandle->stop(0);
        sequenceManager->mSubBgmHandle.releaseSound();
    }
    sequenceManager->bgmStart(
        track, Z2Param::BGM_CROSS_FADEIN_TIME, 0);
    const bool started = sequenceManager->getMainBgmID() == track;
    if (started) {
        sequenceManager->mBgmStatus = 0xff;
    }
    if (!started) {
        log_sit_music_start_failure(
            selectedTrack, sit_music_playback_id(sequenceManager, track));
        return false;
    }
    sequenceManager->field_0xb8 = -1;
    g_sitMusicActiveTrack = selectedTrack;
    if (g_sitMusicLastStartedTrack != selectedTrack) {
        char message[128];
        std::snprintf(message, sizeof(message),
                      "Sit music started: File Select (%08x)", track);
        svc_log->info(mod_ctx, message);
        g_sitMusicLastStartedTrack = selectedTrack;
    }
    return true;
}

void log_sit_music_start_failure(u32 selectedTrack, u32 activeTrack) {
    if (g_sitMusicReportedFailureTrack == selectedTrack) {
        return;
    }

    char message[128];
    std::snprintf(message, sizeof(message),
                  "Sit music File Select (%08x) could not start on its playback handle; active ID is %08x",
                  selectedTrack, activeTrack);
    svc_log->warn(mod_ctx, message);
    g_sitMusicReportedFailureTrack = selectedTrack;
}

void restore_sit_music_battle_override(Z2SeqMgr* sequenceManager) {
    if (!g_sitMusicBattleOverrideActive || sequenceManager == nullptr) {
        return;
    }

    sequenceManager->mBattleSeqCount = 0;
    sequenceManager->mBattleSeqState = 0;
    sequenceManager->setBattleBgmOff(g_sitMusicOriginalBattleBgmOff);
    g_sitMusicBattleOverrideActive = false;
}

bool restore_sit_music_scene_waves() {
    if (!g_sitMusicWaveSnapshot.active) {
        auto* audioManager = Z2GetAudioMgr();
        auto* sequenceManager = audioManager != nullptr
            ? static_cast<Z2SeqMgr*>(audioManager) : nullptr;
        restore_sit_music_battle_override(sequenceManager);
        return true;
    }

    auto* audioManager = Z2GetAudioMgr();
    auto* sequenceManager = audioManager != nullptr
        ? static_cast<Z2SeqMgr*>(audioManager) : nullptr;
    auto* sceneManager = audioManager != nullptr
        ? static_cast<Z2SceneMgr*>(audioManager) : nullptr;
    if (sequenceManager == nullptr || sceneManager == nullptr) {
        return false;
    }
    auto* sectionHeap = JAUSectionHeap::getInstance();
    auto* waveBank = sectionHeap != nullptr
        ? sectionHeap->getWaveBankTable().getWaveBank(1) : nullptr;
    if (waveBank == nullptr) {
        return false;
    }
    for (u32 wave = 1; wave < g_sitMusicOwnedWaves.size(); ++wave) {
        if (!g_sitMusicOwnedWaves[wave]) {
            continue;
        }

        const s32 status = sceneManager->getBgmLoadStatus(wave);
        if (status == 1) {
            return false;
        }
        if (status == 2 && !sceneManager->eraseBgmWave(wave)) {
            return false;
        }
        g_sitMusicOwnedWaves[wave] = false;
        g_sitMusicWaveReleaseScheduled[wave] = false;
        g_sitMusicWaveReleaseFrames[wave] = 0;
    }

    const u8 sceneWaves[] = {
        g_sitMusicWaveSnapshot.loadedWave1,
        g_sitMusicWaveSnapshot.loadedWave2,
    };
    for (u8 wave : sceneWaves) {
        if (wave == 0) {
            continue;
        }

        s32 status = sceneManager->getBgmLoadStatus(wave);
        if (status == 0) {
            if (!load_sit_music_wave(sceneManager, wave)) {
                status = sceneManager->getBgmLoadStatus(wave);
                if (status == 0) {
                    if (!g_sitMusicRestoreFailureReported) {
                        char message[96];
                        std::snprintf(
                            message, sizeof(message),
                            "Could not reload original Sit music wave %02x",
                            wave);
                        svc_log->error(mod_ctx, message);
                        g_sitMusicRestoreFailureReported = true;
                    }
                    return false;
                }
            } else {
                return false;
            }
        }
        if (status == 1) {
            return false;
        }
        if (status != 2) {
            return false;
        }
    }

    const SitMusicWaveSnapshot snapshot = g_sitMusicWaveSnapshot;
    sceneManager->requestBgmWave_1 = snapshot.requestWave1;
    sceneManager->requestBgmWave_2 = snapshot.requestWave2;
    sceneManager->loadedBgmWave_1 = snapshot.loadedWave1;
    sceneManager->loadedBgmWave_2 = snapshot.loadedWave2;

    if (snapshot.mainTrack != static_cast<u32>(-1)) {
        sequenceManager->bgmStart(
            snapshot.mainTrack, Z2Param::BGM_CROSS_FADEIN_TIME, 0);
        if (snapshot.mainStatus != 0xff) {
            sequenceManager->changeBgmStatus(snapshot.mainStatus);
        }
    } else {
        sequenceManager->mMainBgmMaster.fadeIn(
            Z2Param::BGM_CROSS_FADEIN_TIME);
    }
    restore_sit_music_battle_override(sequenceManager);
    g_sitMusicWaveSnapshot = {};
    g_sitMusicWaveTrack = 0;
    g_sitMusicResolvedWave1 = 0;
    g_sitMusicRestoreFailureReported = false;
    return true;
}

void stop_sit_music_track() {
    auto* audioManager = Z2GetAudioMgr();
    auto* sequenceManager = audioManager != nullptr
        ? static_cast<Z2SeqMgr*>(audioManager) : nullptr;
    if (sequenceManager == nullptr) {
        return;
    }

    if (g_sitMusicAreaFadePending) {
        sequenceManager->mMainBgmMaster.fadeIn(
            Z2Param::BGM_CROSS_FADEIN_TIME);
        g_sitMusicAreaFadePending = false;
        g_sitMusicAreaFadeFrames = 0;
        g_sitMusicAreaFadeTimeoutReported = false;
    }

    const u32 track = g_sitMusicActiveTrack;
    if (track != 0 && sequenceManager->mFanfareID == track) {
        sequenceManager->mFanfareID = static_cast<JAISoundID>(-1);
    }
    if (track != 0 && sequenceManager->mFanfareHandle &&
        sequenceManager->mFanfareHandle->getID() == track)
    {
        sequenceManager->mFanfareHandle->stop(15);
        sequenceManager->mFanfareHandle.releaseSound();
        sequenceManager->mFanfareCount = 0;
        sequenceManager->mFanfareMute.fadeIn(15);
    }
    if (track != 0) {
        if (sequenceManager->getMainBgmID() == track) {
            sequenceManager->mMainBgmHandle->stop(15);
            sequenceManager->mMainBgmHandle.releaseSound();
        }
        if (sequenceManager->getSubBgmID() == track) {
            sequenceManager->mSubBgmHandle->stop(15);
            sequenceManager->mSubBgmHandle.releaseSound();
        }
        if (!g_sitMusicWaveSnapshot.active) {
            sequenceManager->mMainBgmMaster.fadeIn(15);
        }
        sequenceManager->field_0xb8 = -1;
    }
    if (!g_sitMusicWaveSnapshot.active) {
        schedule_sit_music_wave_releases(0, 30);
        restore_sit_music_battle_override(sequenceManager);
    }
    g_sitMusicActiveTrack = 0;
}

void update_sit_music() {
    update_sit_music_wave_cleanup();
    if (g_sitActionState != SitActionState::Seated) {
        stop_sit_music_track();
        restore_sit_music_scene_waves();
        g_sitMusicReportedFailureTrack = 0;
        g_sitMusicWaveFailureTrack = 0;
        return;
    }

    const int64_t selection = sitMusicSelection();
    if (selection != g_sitMusicLastLoggedSelection) {
        g_sitMusicLastLoggedSelection = selection;
        svc_log->info(
            mod_ctx,
            selection == 0
                ? "Sit music selector read: File Select"
                : "Sit music selector unavailable");
    }
    if (selection < 0) {
        stop_sit_music_track();
        restore_sit_music_scene_waves();
        g_sitMusicReportedFailureTrack = 0;
        g_sitMusicWaveFailureTrack = 0;
        return;
    }

    auto* audioManager = Z2GetAudioMgr();
    auto* sequenceManager = audioManager != nullptr
        ? static_cast<Z2SeqMgr*>(audioManager) : nullptr;
    if (sequenceManager == nullptr) {
        if (!g_sitMusicAudioManagerUnavailableReported) {
            svc_log->error(mod_ctx, "game audio manager is unavailable for Sit music");
            g_sitMusicAudioManagerUnavailableReported = true;
        }
        return;
    }
    g_sitMusicAudioManagerUnavailableReported = false;
    if (g_sitMusicBattleOverrideActive) {
        sequenceManager->setBattleBgmOff(true);
        sequenceManager->mBattleSeqCount = 0;
    }

    const u32 track = kSitMusicBgmId;
    const u32 expectedTrack = track;
    if (g_sitMusicWaveSnapshot.active && g_sitMusicWaveTrack != track) {
        stop_sit_music_track();
        restore_sit_music_scene_waves();
        return;
    }
    if (g_sitMusicActiveTrack != 0 && g_sitMusicActiveTrack != track) {
        stop_sit_music_track();
        if (g_sitMusicWaveSnapshot.active) {
            restore_sit_music_scene_waves();
            return;
        }
    }
    if (g_sitMusicWaveFailureTrack != track) {
        g_sitMusicWaveFailureTrack = 0;
    }
    if (g_sitMusicReportedFailureTrack != track) {
        g_sitMusicReportedFailureTrack = 0;
    }
    if (g_sitMusicReportedFailureTrack == track ||
        g_sitMusicWaveFailureTrack == track)
    {
        stop_sit_music_track();
        restore_sit_music_scene_waves();
        return;
    }
    if (g_sitMusicActiveTrack == track &&
        sit_music_playback_id(sequenceManager, expectedTrack) == expectedTrack)
    {
        return;
    }
    if (g_sitMusicActiveTrack == track) {
        g_sitMusicActiveTrack = 0;
        if (g_sitMusicReportedOverrideTrack != track) {
            char message[128];
            std::snprintf(message, sizeof(message),
                          "Sit music handle changed after start: expected %08x, got %08x",
                          expectedTrack,
                          sit_music_playback_id(sequenceManager, expectedTrack));
            svc_log->warn(mod_ctx, message);
            g_sitMusicReportedOverrideTrack = track;
        }
    }

    if (!g_sitMusicAreaFadePending) {
        sequenceManager->mMainBgmMaster.fadeOut(30);
        g_sitMusicAreaFadePending = true;
        g_sitMusicAreaFadeFrames = 0;
        g_sitMusicAreaFadeTimeoutReported = false;
    }
    if (sequenceManager->mMainBgmMaster.getCount() != 0 ||
        sequenceManager->mMainBgmMaster.getIntensity() > 0.001f)
    {
        constexpr u16 kSitMusicFadeTimeoutFrames = 90;
        if (++g_sitMusicAreaFadeFrames < kSitMusicFadeTimeoutFrames) {
            return;
        }
        sequenceManager->mMainBgmMaster.forceOut();
        if (!g_sitMusicAreaFadeTimeoutReported) {
            svc_log->warn(
                mod_ctx,
                "Sit music area fade timed out; forcing playback startup");
            g_sitMusicAreaFadeTimeoutReported = true;
        }
    }

    if (start_sit_music_track(track)) {
        g_sitMusicAreaFadePending = false;
        g_sitMusicAreaFadeFrames = 0;
        g_sitMusicAreaFadeTimeoutReported = false;
    }
}

void request_sit_action(daAlink_c* alink) {
    begin_sit_action(alink);
}
