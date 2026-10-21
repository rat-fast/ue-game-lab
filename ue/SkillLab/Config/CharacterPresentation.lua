-- SkillLab character presentation policy.
-- Runtime facts come from C++; UE/AnimBP owns pose evaluation and asset playback.

local lastBaseState = nil

function OnBaseStateChanged(state, transitionSerial, bShouldMove, bIsFalling)
    if state ~= lastBaseState then
        SkillLabPresentation.Log(string.format("base state=%s transition=%d", state, transitionSerial))
        lastBaseState = state
    end

    -- Translate the authoritative state snapshot to the legacy AnimBP inputs.
    -- Velocity and pose evaluation remain owned by C++ and Unreal animation.
    local falling = (state == "Jump" or state == "Fall") and bIsFalling
    local moving = (state == "Move" or state == "Jump" or state == "Fall" or state == "Land") and bShouldMove
    SkillLabPresentation.SetLocomotionIntent(moving, falling)
    SkillLabPresentation.Log(string.format("locomotion intent state=%s move=%s falling=%s",
        state, tostring(moving), tostring(falling)))
end

function OnSkillEvent(event)
    -- A requested action only follows an authoritative Started event.
    if event.event == "Started" then
        local played = SkillLabPresentation.PlaySkillAction(event.castId, event.skillId)
        if not played then
            SkillLabPresentation.Log(string.format(
                "cast=%d skill=%s has no playable action; skill runtime continues",
                event.castId, event.skillId))
        end
    elseif event.event == "Cancelled" or event.event == "Completed" then
        -- C++ checks CastId again before stopping the active UE montage.
        SkillLabPresentation.StopSkillAction(event.castId)
    elseif event.event == "Rejected" then
        SkillLabPresentation.Log(string.format("request rejected reason=%s", event.reason))
    elseif event.event == "Hit" then
        SkillLabPresentation.Log(string.format(
            "cast=%d skill=%s hit success=%s amount=%.1f",
            event.castId, event.skillId, tostring(event.success), event.actualAmount))
    elseif event.event == "PhaseChanged" or event.event == "TargetSelected" or event.event == "EffectApplied" then
        SkillLabPresentation.Log(string.format(
            "cast=%d skill=%s event=%s stage=%s success=%s",
            event.castId, event.skillId, event.event, event.stage, tostring(event.success)))
    elseif event.event == "BuffApplied" or event.event == "DamageTick" or event.event == "BuffRemoved" then
        -- Persistent-effect feedback is independent from the cast action lifetime.
        SkillLabPresentation.Log(string.format(
            "cast=%d skill=%s effect event=%s amount=%.1f",
            event.castId, event.skillId, event.event, event.actualAmount))
    end
end
