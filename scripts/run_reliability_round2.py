#!/usr/bin/env python3

import migrate_reliability_round2 as migration

migration.patch_pattern_paging()
migration.patch_display_paging()
migration.patch_scene_manager()
migration.patch_scene_save_result()
migration.patch_audio_runtime()
