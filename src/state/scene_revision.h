#ifndef GROOVEPUTER_SRC_STATE_SCENE_REVISION_H_
#define GROOVEPUTER_SRC_STATE_SCENE_REVISION_H_

#include <cstdint>

namespace GroovePuterState {

struct SceneRevisionState {
    uint32_t currentRevision{0};
    uint32_t persistedRevision{0};

    bool dirty() const {
        return currentRevision != persistedRevision;
    }

    void markPersistentMutation() {
        ++currentRevision;
        // Preserve the invariant even when uint32_t wraps onto the persisted
        // value. One extra increment is bounded and keeps dirty truthful.
        if (currentRevision == persistedRevision) ++currentRevision;
    }

    void markSaveSucceeded() {
        persistedRevision = currentRevision;
    }

    void markLoadSucceeded() {
        ++currentRevision;
        persistedRevision = currentRevision;
    }
};

static_assert(sizeof(SceneRevisionState) == 8,
              "scene revision tracker must remain an 8-byte runtime service");

inline SceneRevisionState& sceneRevisionState() {
    static SceneRevisionState state{};
    return state;
}

inline SceneRevisionState sceneRevisionSnapshot() {
    return sceneRevisionState();
}

inline void restoreSceneRevision(const SceneRevisionState& snapshot) {
    sceneRevisionState() = snapshot;
}

inline void markSceneMutated() {
    sceneRevisionState().markPersistentMutation();
}

inline void markSceneSaveSucceeded() {
    sceneRevisionState().markSaveSucceeded();
}

inline void markSceneLoadSucceeded() {
    sceneRevisionState().markLoadSucceeded();
}

inline bool sceneDirty() {
    return sceneRevisionState().dirty();
}

}  // namespace GroovePuterState

#endif  // GROOVEPUTER_SRC_STATE_SCENE_REVISION_H_
