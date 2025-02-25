/**
 * @file Player.cpp
 * @date 18-Apr-2020
 */

#include "Player.h"

const orxSTRING Player::GetConfigVar(const orxSTRING _zVar) const
{
    static orxCHAR sacName[32] = {};

    orxString_NPrint(sacName, sizeof(sacName) - 1, "%s%s", GetName(), _zVar);

    return sacName;
}

void orxFASTCALL Player::ResetDash(const orxCLOCK_INFO *_pstClockInfo, void *_pContext)
{
    Player *poPlayer = (Player *)_pContext;
    poPlayer->bIsDashing = orxFALSE;
    poPlayer->SetSpeed(orxVECTOR_0);
    orxSPAWNER *pstSmoke = orxOBJECT_GET_STRUCTURE(poPlayer->GetOrxObject(), SPAWNER);
    if(pstSmoke)
    {
        orxSpawner_Enable(pstSmoke, orxFALSE);
    }
}

void orxFASTCALL Player::UpdateBurnRate(const orxCLOCK_INFO *_pstClockInfo, void *_pContext)
{
    Player *poPlayer = (Player *)_pContext;
    if(!poPlayer->bIsDead)
    {
        poPlayer->PushConfigSection();
        poPlayer->u32BurnRateIndex = orxMIN((orxU32)orxConfig_GetListCount("LampBurnRate") - 1, poPlayer->u32BurnRateIndex + 1);
        orxClock_AddGlobalTimer(UpdateBurnRate, orxConfig_GetFloat("LampCapacity") / orxConfig_GetListFloat("LampBurnRate", poPlayer->u32BurnRateIndex), 1, poPlayer);
        poPlayer->PopConfigSection();
    }
}

void Player::OnCreate()
{
    Object::OnCreate();
    orxConfig_SetBool("IsPlayer", orxTRUE);

    orxSPAWNER *pstSmoke = orxOBJECT_GET_STRUCTURE(GetOrxObject(), SPAWNER);
    if(pstSmoke)
    {
        orxSpawner_Enable(pstSmoke, orxFALSE);
    }

    // Inits vars
    zLastAnim           = orxNULL;
    u32BurnRateIndex    = 0;
    bIsDashing          = orxFALSE;
    bIsDead             = orxFALSE;
    bIsDashQueued       = orxFALSE;

    // Enables its inputs
    orxInput_EnableSet(orxConfig_GetString("Input"), orxTRUE);

    // Inits lamp
    orxFLOAT fLampCapacity = orxConfig_GetFloat("LampCapacity");
    orxConfig_PushSection("Runtime");
    orxConfig_SetFloat(GetConfigVar("Oil"), fLampCapacity);
    //orxLOG("%s: [INIT] %g", GetName(), orxConfig_GetFloat(GetConfigVar("Oil")));
    orxConfig_PopSection();

    // Inits burn rate
    orxClock_AddGlobalTimer(UpdateBurnRate, orxConfig_GetFloat("LampCapacity") / orxConfig_GetListFloat("LampBurnRate", u32BurnRateIndex), 1, this);
}

void Player::OnDelete()
{
    Object::OnDelete();

    // Removes all timers
    orxClock_RemoveGlobalTimer(orxNULL, orx2F(-1.0f), this);
}

void Player::Update(const orxCLOCK_INFO &_rstInfo)
{
    // Follow the train
    orxVECTOR vPos;
    GetPosition(vPos, orxTRUE);
    vPos.fZ -= orxFLOAT_1;
    ScrollObject *poTrain = ld46::GetInstance().PickObject(vPos, orxString_ToCRC("Train"));
    if(poTrain)
    {
        if(orxObject_GetParent(GetOrxObject()) != (orxSTRUCTURE *)poTrain->GetOrxObject())
        {
            orxObject_Attach(GetOrxObject(), poTrain->GetOrxObject());
        }
    }
    else
    {
        orxObject_Detach(GetOrxObject());
    }

    // Not dead?
    if(!bIsDead)
    {
        PushConfigSection();

        // Update base class
        Object::Update(_rstInfo);

        // Update lamp
        orxFLOAT fLampBurnRate = orxConfig_GetListFloat("LampBurnRate", u32BurnRateIndex);
        orxConfig_PushSection("Runtime");
        const orxSTRING zOil = GetConfigVar("Oil");
        orxFLOAT fLampOil = orxConfig_GetFloat(zOil);
        fLampOil = orxMAX(fLampOil - fLampBurnRate * _rstInfo.fDT, orxFLOAT_0);
        //orxLOG("%s: [BURN] %g -> %g / Rate %g / DT %g", GetName(), orxConfig_GetFloat(zOil), fLampOil, fLampBurnRate, _rstInfo.fDT);
        orxConfig_SetFloat(zOil, fLampOil);
        zOil = GetConfigVar("RateIndex");
        orxConfig_SetU32(zOil, u32BurnRateIndex);
        orxConfig_PopSection();

        // Update light
        orxVECTOR vScale = {orxMAX(fLampOil, orxMATH_KF_EPSILON), orxMAX(fLampOil, orxMATH_KF_EPSILON), orxFLOAT_0};
        GetOwnedChild()->SetScale(vScale);

        // No more oil?
        if(fLampOil == orxFLOAT_0)
        {
            // Game Over
            SetSpeed(orxVECTOR_0);
            SetAnim("Death");
            bIsDead = orxTRUE;
            orxClock_RemoveGlobalTimer(orxNULL, orx2F(-1.0f), this);
        }
        else
        {
            // Select input set
            const orxSTRING zSet = orxInput_GetCurrentSet();
            orxInput_SelectSet(orxConfig_GetString("Input"));

            // Not dashing?
            if(!bIsDashing)
            {
                // Move
                orxVECTOR vSpeed = {orxInput_GetValue("MoveRight") - orxInput_GetValue("MoveLeft"), orxInput_GetValue("MoveDown") - orxInput_GetValue("MoveUp"), orxFLOAT_0};

                // Dash
                if(bIsDashQueued || orxInput_HasBeenActivated("Dash"))
                {
                    if(!orxVector_IsNull(&vSpeed))
                    {
                        orxVector_Mulf(&vSpeed, orxVector_Normalize(&vSpeed, &vSpeed), orxConfig_GetFloat("DashSpeed"));
                        bIsDashing = orxTRUE;
                        orxClock_AddGlobalTimer(ResetDash, orxConfig_GetFloat("DashDuration"), 1, this);
                        ld46::GetInstance().CreateObject("DashSound");
                        orxSPAWNER *pstSmoke = orxOBJECT_GET_STRUCTURE(GetOrxObject(), SPAWNER);
                        if(pstSmoke)
                        {
                            orxSpawner_Enable(pstSmoke, orxTRUE);
                        }
                    }

                    bIsDashQueued = orxFALSE;
                }
                else
                {
                    orxVector_Mulf(&vSpeed, &vSpeed, orxConfig_GetFloat("Speed"));
                }

                // Set speed
                SetSpeed(vSpeed);

                // Select anim
                if(vSpeed.fX < orxFLOAT_0)
                {
                    zLastAnim = "RunLeft";
                }
                else if(vSpeed.fX > orxFLOAT_0)
                {
                    zLastAnim = "RunRight";
                }

                // Update Anim
                SetAnim((orxVector_GetSize(&vSpeed) > orxConfig_GetFloat("MoveThreshold")) ? zLastAnim : orxNULL);
            }
            else
            {
                // Dash?
                if(orxInput_HasBeenActivated("Dash"))
                {
                    // Queue dash
                    bIsDashQueued = orxTRUE;
                }
            }

            // Deselect input set
            orxInput_SelectSet(zSet);
        }

        PopConfigSection();
    }
}

void Player::OnCollide(ScrollObject *_poCollider, orxBODY_PART *_pstPart, orxBODY_PART *_pstColliderPart, const orxVECTOR &_rvPosition, const orxVECTOR &_rvNormal)
{
    if(!bIsDead)
    {
        if(orxString_SearchString(orxBody_GetPartName(_pstColliderPart), "Oil"))
        {
            PushConfigSection();

            orxFLOAT fLampRefill = orxConfig_GetFloat("LampRefill");
            orxFLOAT fLampCapacity = orxConfig_GetFloat("LampCapacity");

            orxConfig_PushSection("Runtime");

            const orxSTRING zOil = GetConfigVar("Oil");
            orxFLOAT fLampOil = orxConfig_GetFloat(zOil);
            fLampOil = orxMIN(fLampOil + fLampRefill, fLampCapacity);
            //orxLOG("%s: [COLLIDE] %g -> %g", GetName(), orxConfig_GetFloat(zOil), fLampOil);
            orxConfig_SetFloat(zOil, fLampOil);

            orxConfig_SetU64("Collider", GetGUID());

            orxConfig_PopSection();

            _poCollider->AddConditionalTrack("PickUp");

            PopConfigSection();
        }
        else if(orxString_SearchString(orxBody_GetPartName(_pstColliderPart), "Death"))
        {
            orxConfig_PushSection("Runtime");
            orxConfig_SetFloat(GetConfigVar("Oil"), orxFLOAT_0);
            orxConfig_PopSection();
        }
        else if(bIsDashing)
        {
            ld46::GetInstance().CreateObject("HitSound");
        }
    }
}
