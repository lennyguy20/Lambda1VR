/************************************************************************************

Filename	:	VrInputAlt2.c
Content		:	Handles controller input for the second alternative
Created		:	August 2019
Authors		:	Simon Brown

*************************************************************************************/

#include <common/common.h>
#include <common/library.h>
#include <common/cvardef.h>
#include <common/xash3d_types.h>
#include <engine/keydefs.h>
#include <client/touch.h>
#include <client/client.h>

#include "VrInput.h"
#include "VrCvars.h"
#include "../Xash3D/xash3d/engine/client/client.h"

extern cvar_t	*cl_forwardspeed;
extern cvar_t	*cl_movespeedkey;

void Touch_Motion( touchEventType type, int fingerID, float x, float y, float dx, float dy );

extern float initialTouchX, initialTouchY;


void HandleInput_OneController( ovrInputStateTrackedRemote *pDominantTrackedRemoteNew, ovrInputStateTrackedRemote *pDominantTrackedRemoteOld, ovrTrackedController* pDominantTracking,
                          ovrInputStateTrackedRemote *pOffTrackedRemoteNew, ovrInputStateTrackedRemote *pOffTrackedRemoteOld, ovrTrackedController* pOffTracking,
                          int domButton1, int domButton2, int offButton1, int offButton2 )
{
	//Ensure handedness is set correctly
	const char* pszHand = (vr_control_scheme->integer >= 10) ? "1" : "0";
	Cvar_Set("hand", pszHand);


	static bool shifted = false;
	static bool dominantGripPushed = false;
	static float dominantGripPushTime = 0.0f;

	//Menu button - always on the left controller - unavoidable
	handleTrackedControllerButton(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, xrButton_Enter, K_ESCAPE);

    //Menu control - Uses "touch"
	if (VR_UseScreenLayer())
	{
        interactWithTouchScreen(pDominantTracking, pDominantTrackedRemoteNew, pDominantTrackedRemoteOld);
	}
	else
	{
		//If distance to off-hand remote is less than 35cm and user pushes grip, then we enable weapon stabilisation
		float distance = sqrtf(powf(pOffTracking->Pose.position.x - pDominantTracking->Pose.position.x, 2) +
							   powf(pOffTracking->Pose.position.y - pDominantTracking->Pose.position.y, 2) +
							   powf(pOffTracking->Pose.position.z - pDominantTracking->Pose.position.z, 2));

		float distanceToHMD = sqrtf(powf(hmdPosition[0] - pDominantTracking->Pose.position.x, 2) +
									powf(hmdPosition[1] - pDominantTracking->Pose.position.y, 2) +
									powf(hmdPosition[2] - pDominantTracking->Pose.position.z, 2));

		static float forwardYaw = 0;
		if (!isScopeEngaged())
		{
			forwardYaw = (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]) - snapTurn;
		}

		//dominant hand stuff first
		{
			///Weapon location relative to view
			weaponoffset[0] = pDominantTracking->Pose.position.x - hmdPosition[0];
			weaponoffset[1] = pDominantTracking->Pose.position.y - hmdPosition[1];
			weaponoffset[2] = pDominantTracking->Pose.position.z - hmdPosition[2];

			{
				vec2_t v;
				rotateAboutOrigin(weaponoffset[0], weaponoffset[2], -(cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]), v);
				weaponoffset[0] = v[0];
				weaponoffset[2] = v[1];

				ALOGV("        Weapon Offset: %f, %f, %f",
					  weaponoffset[0],
					  weaponoffset[1],
					  weaponoffset[2]);
			}

			//Weapon velocity
			weaponvelocity[0] = pDominantTracking->Velocity.linearVelocity.x;
			weaponvelocity[1] = pDominantTracking->Velocity.linearVelocity.y;
			weaponvelocity[2] = pDominantTracking->Velocity.linearVelocity.z;

			{
				vec2_t v;
				rotateAboutOrigin(weaponvelocity[0], weaponvelocity[2], -cl.refdef.cl_viewangles[YAW], v);
				weaponvelocity[0] = v[0];
				weaponvelocity[2] = v[1];

				ALOGV("        Weapon Velocity: %f, %f, %f",
					  weaponvelocity[0],
					  weaponvelocity[1],
					  weaponvelocity[2]);
			}


			//Set gun angles
            vec3_t rotation = {0};
            rotation[PITCH] = vr_weapon_pitchadjust->value;
            QuatToYawPitchRoll(pDominantTracking->Pose.orientation, rotation, weaponangles[ADJUSTED]);
            rotation[PITCH] = 0.0f;
            QuatToYawPitchRoll(pDominantTracking->Pose.orientation, rotation, weaponangles[UNADJUSTED]);
            rotation[PITCH] = vr_crowbar_pitchadjust->value;
            QuatToYawPitchRoll(pDominantTracking->Pose.orientation, rotation, weaponangles[MELEE]);

			weaponangles[ADJUSTED][YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);
			weaponangles[ADJUSTED][PITCH] *= -1.0f;

			weaponangles[UNADJUSTED][YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);
			weaponangles[UNADJUSTED][PITCH] *= -1.0f;

			weaponangles[MELEE][YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);
			weaponangles[MELEE][PITCH] *= -1.0f;

			static bool finishReloadNextFrame = false;
			if (finishReloadNextFrame)
			{
				sendButtonActionSimple("-reload");
				finishReloadNextFrame = false;
			}

			if ((pDominantTrackedRemoteNew->Buttons & xrButton_GripTrigger) !=
				(pDominantTrackedRemoteOld->Buttons & xrButton_GripTrigger)) {

				dominantGripPushed = (pDominantTrackedRemoteNew->Buttons &
									  xrButton_GripTrigger) != 0;

				if (!isBackpack(pDominantTracking)) {

					if (dominantGripPushed) {
						dominantGripPushTime = TBXR_GetTimeInMilliSeconds();
					} else {
						if ((TBXR_GetTimeInMilliSeconds() - dominantGripPushTime) <
							vr_reloadtimeoutms->integer) {
							sendButtonActionSimple("+reload");
							finishReloadNextFrame = true;
						}
					}
				}
			}
		}

		float controllerYawHeading = 0.0f;

		//still use off hand controller for flash light
        if (vr_headtorch->value == 1.0f)
        {
            VectorSet(flashlightoffset, 0, 0, 0);
            VectorCopy(hmdorientation, flashlightangles);
            VectorCopy(hmdorientation, offhandangles);
        }
        else
        {
            flashlightoffset[0] = pOffTracking->Pose.position.x - hmdPosition[0];
            flashlightoffset[1] = pOffTracking->Pose.position.y - hmdPosition[1];
            flashlightoffset[2] = pOffTracking->Pose.position.z - hmdPosition[2];

            vec2_t v;
            rotateAboutOrigin(flashlightoffset[0], flashlightoffset[2],
                              -(cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]), v);
            flashlightoffset[0] = v[0];
            flashlightoffset[2] = v[1];

            vec3_t rotation = {0};
            QuatToYawPitchRoll(pOffTracking->Pose.orientation, rotation, offhandangles);
            rotation[PITCH] = 15.0f;
            QuatToYawPitchRoll(pOffTracking->Pose.orientation, rotation, flashlightangles);
            rotation[PITCH] = 25.0f;
            vec3_t inverseflashlightangles;
            QuatToYawPitchRoll(pOffTracking->Pose.orientation, rotation, inverseflashlightangles);
            if (vr_reversetorch->integer)
            {
                VectorNegate(inverseflashlightangles, flashlightangles);
                flashlightangles[YAW] *= -1.0f;
                flashlightangles[YAW] += 180.f;
            }
        }

        flashlightangles[YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);
        offhandangles[YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);

        controllerYawHeading = 0.0f;


		{
			//This section corrects for the fact that the controller actually controls direction of movement, but we want to move relative to the direction the
			//player is facing for positional tracking
			float multiplier = vr_positional_factor->value / (cl_forwardspeed->value *
					((pOffTrackedRemoteNew->Buttons & xrButton_Trigger) ? cl_movespeedkey->value : 1.0f));

			//If player is ducked then multiply by 3 otherwise positional tracking feels very wrong
			if (ducked != DUCK_NOTDUCKED)
			{
				multiplier *= 3.0f;
			}

			vec2_t v;
			rotateAboutOrigin(-positionDeltaThisFrame[0] * multiplier,
							  positionDeltaThisFrame[2] * multiplier, -hmdorientation[YAW], v);
			positional_movementSideways = v[0];
			positional_movementForward = v[1];

			ALOGV("        positional_movementSideways: %f, positional_movementForward: %f",
				  positional_movementSideways,
				  positional_movementForward);

            shifted = (dominantGripPushed &&
                (TBXR_GetTimeInMilliSeconds() - dominantGripPushTime) > vr_reloadtimeoutms->integer);

            static bool using = false;
            static bool weaponSwitched = false;
            if (shifted && !using) {
                if (between(0.75f, pDominantTrackedRemoteNew->Joystick.y, 1.0f) ||
                    between(-1.0f, pDominantTrackedRemoteNew->Joystick.y, -0.75f) ||
                    between(0.75f, pDominantTrackedRemoteNew->Joystick.x, 1.0f) ||
                    between(-1.0f, pDominantTrackedRemoteNew->Joystick.x, -0.75f))
                {
                    if (!weaponSwitched) {
                        if (!weaponSwitched)
                        {
                            if (between(0.75f, pDominantTrackedRemoteNew->Joystick.y, 1.0f))
                            {
                                sendButtonActionSimple("invnext"); // in this mode is makes more sense to select the next item with Joystick down; this is the mouse wheel behaviour in hl
                            }
                            else if (between(-1.0f, pDominantTrackedRemoteNew->Joystick.y, -0.75f))
                            {
                                sendButtonActionSimple("invprev");
                            }
                            else if (between(0.75f, pDominantTrackedRemoteNew->Joystick.x, 1.0f))
                            {
                                sendButtonActionSimple("invprevslot"); // not an original hl methode -> needs update from hlsdk-xash3d
                            }
                            else if (between(-1.0f, pDominantTrackedRemoteNew->Joystick.x, -0.75f))
                            {
                                sendButtonActionSimple("invnextslot");
                            }
                            weaponSwitched = true;
                        }
                        weaponSwitched = true;
                    }
                }
                else {
                    weaponSwitched = false;
                }
            }
            else {

                static float joyx[4] = {0};
                static float joyy[4] = {0};
                for (int j = 3; j > 0; --j) {
                    joyx[j] = joyx[j - 1];
                    joyy[j] = joyy[j - 1];
                }
                joyx[0] = pDominantTrackedRemoteNew->Joystick.x;
                joyy[0] = pDominantTrackedRemoteNew->Joystick.y;
                float joystickX = (joyx[0] + joyx[1] + joyx[2] + joyx[3]) / 4.0f;
                float joystickY = (joyy[0] + joyy[1] + joyy[2] + joyy[3]) / 4.0f;

                //Apply a filter and quadratic scaler so small movements are easier to make
                float dist = length(joystickX,
                                    joystickY);
                float nlf = nonLinearFilter(dist);
                float x = nlf * joystickX;
                float y = nlf * joystickY;

                player_moving = (fabs(x) + fabs(y)) > 0.01f;


                //Adjust to be off-hand controller oriented
                vec2_t v;
                rotateAboutOrigin(x, y, controllerYawHeading, v);

                remote_movementSideways = v[0];
                remote_movementForward = v[1];

                ALOGV("        remote_movementSideways: %f, remote_movementForward: %f",
                      remote_movementSideways,
                      remote_movementForward);
            }

            //Fire Primary
            if ((pDominantTrackedRemoteNew->Buttons & xrButton_Trigger) !=
                (pDominantTrackedRemoteOld->Buttons & xrButton_Trigger)) {

                bool firingPrimary = (pDominantTrackedRemoteNew->Buttons & xrButton_Trigger);
                sendButtonAction("+attack", firingPrimary);
            }

            //Laser Sight
            if ((pOffTrackedRemoteNew->Buttons & xrButton_Joystick) !=
                (pOffTrackedRemoteOld->Buttons & xrButton_Joystick)
                && (pOffTrackedRemoteNew->Buttons & xrButton_Joystick)) {

                Cvar_SetFloat("vr_lasersight", (int)(vr_lasersight->value + 1) % 3);

            }

            //flashlight on/off
            if (((pOffTrackedRemoteNew->Buttons & offButton2) !=
                 (pOffTrackedRemoteOld->Buttons & offButton2)) &&
                (pOffTrackedRemoteNew->Buttons & offButton2)) {
                sendButtonActionSimple("impulse 100");
            }

            static int  jumpMode = 0;
            static bool running = false;
            static int jumpTimer = 0;
            static bool firingSecondary = false;
            static bool duckToggle = false;
            if (shifted) {
                //If we were in the process of firing secondary, then just stop, otherwise it
                //can get confused
                if (firingSecondary)
                {
                    firingSecondary = false;
                    sendButtonActionSimple("-attack2");
                }

                jumpTimer = 0;
                //Switch Jump Mode
                if (((pDominantTrackedRemoteNew->Buttons & domButton1) !=
                     (pDominantTrackedRemoteOld->Buttons & domButton1)) &&
                    (pDominantTrackedRemoteNew->Buttons & domButton1)) {
                    jumpMode = 1 - jumpMode;
                    if (jumpMode == 0) {
                        CL_CenterPrint("JumpMode:  Jump {-> Duck}", -1);
                    } else {
                        CL_CenterPrint("JumpMode:  Duck -> Jump", -1);
                    }

                }

                //Run - toggle
                if ((pDominantTrackedRemoteNew->Buttons & domButton2) !=
                    (pDominantTrackedRemoteOld->Buttons & domButton2) &&
                    (pDominantTrackedRemoteNew->Buttons & domButton2)) {
                    running = !running;
                    if (running) {
                        CL_CenterPrint("Run Toggle: Enabled", -1);
                    } else {
                        CL_CenterPrint("Run Toggle: Disabled", -1);
                    }
                }

                //Use (Action)
                if ((pDominantTrackedRemoteNew->Buttons & xrButton_Joystick) !=
                    (pDominantTrackedRemoteOld->Buttons & xrButton_Joystick)) {

                    using = (pDominantTrackedRemoteNew->Buttons & xrButton_Joystick);
                    sendButtonAction("+use", (pDominantTrackedRemoteNew->Buttons & xrButton_Joystick));
                }
            }
            else
            {
                using = false;

                //Fire Secondary
                if ((pDominantTrackedRemoteNew->Buttons & domButton2) !=
                    (pDominantTrackedRemoteOld->Buttons & domButton2)) {

                    firingSecondary = (pDominantTrackedRemoteNew->Buttons & domButton2) > 0;
                    sendButtonAction("+attack2", (pDominantTrackedRemoteNew->Buttons & domButton2));
                }

                //Start of Jump logic - initialise timer
                if (((pDominantTrackedRemoteNew->Buttons & xrButton_Joystick) !=
                     (pDominantTrackedRemoteOld->Buttons & xrButton_Joystick)))
                {
                    if (pDominantTrackedRemoteNew->Buttons & xrButton_Joystick)
                    {
                        jumpTimer = TBXR_GetTimeInMilliSeconds();
                    }
                }

                if (jumpMode == 0) {

                    //Jump Mode 0:  Jump -> {Duck}
					sendButtonAction("+jump", pDominantTrackedRemoteNew->Buttons & xrButton_Joystick);

                    //We are in jump macro
                    if (jumpTimer != 0) {
                        if (TBXR_GetTimeInMilliSeconds() - jumpTimer > 250) {
                            duckToggle = false; // Control of ducking now down to this

                            ducked = (pDominantTrackedRemoteNew->Buttons & xrButton_Joystick) ? DUCK_BUTTON : DUCK_NOTDUCKED;
                            sendButtonAction("+duck", (pDominantTrackedRemoteNew->Buttons & xrButton_Joystick));
                        }

                        if (!(pDominantTrackedRemoteNew->Buttons & xrButton_Joystick)) {
                            jumpTimer = 0;
                        }
                    }

                } else {
                    //We are in jump macro
                    if (jumpTimer != 0) {
                        duckToggle = false; // Control of ducking now down to this

                        //Jump Mode 1:  Duck -> Jump
                        ducked = (pDominantTrackedRemoteNew->Buttons & xrButton_Joystick)
                                 ? DUCK_BUTTON : DUCK_NOTDUCKED;
                        sendButtonAction("+duck", (pDominantTrackedRemoteNew->Buttons &
                                                   xrButton_Joystick));

                        if (TBXR_GetTimeInMilliSeconds() - jumpTimer > 250) {
							sendButtonAction("+jump", pDominantTrackedRemoteNew->Buttons & xrButton_Joystick);
						}

                        if (!(pDominantTrackedRemoteNew->Buttons & xrButton_Joystick)) {
                            jumpTimer = 0;
                        }
                    }
                }

                if (jumpTimer == 0) {
                    //Duck
                    if (((pDominantTrackedRemoteNew->Buttons & domButton1) != (pDominantTrackedRemoteOld->Buttons & domButton1)) &&
                        (pDominantTrackedRemoteOld->Buttons & domButton1) &&
                        ducked != DUCK_CROUCHED) {
                        ducked = (pDominantTrackedRemoteNew->Buttons & domButton1) ? DUCK_BUTTON
                                                                                   : DUCK_NOTDUCKED;

                        duckToggle = !duckToggle;
                        sendButtonAction("+duck", duckToggle ? 1 : 0);
                    }
                }
            }

			// Use (Action gesture)
			if (vr_gesture_triggered_use->integer) {
				bool gestureUseAllowed = !vr_weapon_stabilised->value;
				// Off-hand gesture
				float xOffset = hmdPosition[0] - pOffTracking->Pose.position.x;
				float zOffset = hmdPosition[2] - pOffTracking->Pose.position.z;
				float distanceToBody = sqrtf((xOffset * xOffset) + (zOffset * zOffset));
				if (gestureUseAllowed && (distanceToBody > vr_use_gesture_boundary->value)) {
					if (!(use_gesture_state & VR_USE_GESTURE_OFF_HAND)) {
						sendButtonAction("+use2", true);
					}
					use_gesture_state |= VR_USE_GESTURE_OFF_HAND;
				} else {
					if (use_gesture_state & VR_USE_GESTURE_OFF_HAND) {
						sendButtonAction("+use2", false);
					}
					use_gesture_state &= ~VR_USE_GESTURE_OFF_HAND;
				}
				// Weapon-hand gesture
				xOffset = hmdPosition[0] - pDominantTracking->Pose.position.x;
				zOffset = hmdPosition[2] - pDominantTracking->Pose.position.z;
				distanceToBody = sqrtf((xOffset * xOffset) + (zOffset * zOffset));
				if (gestureUseAllowed && (distanceToBody > vr_use_gesture_boundary->value)) {
					if (!(use_gesture_state & VR_USE_GESTURE_WEAPON_HAND)) {
						sendButtonAction("+use", true);
					}
					use_gesture_state |= VR_USE_GESTURE_WEAPON_HAND;
				} else {
					if (use_gesture_state & VR_USE_GESTURE_WEAPON_HAND) {
						sendButtonAction("+use", false);
					}
					use_gesture_state &= ~VR_USE_GESTURE_WEAPON_HAND;
				}
			} else {
				if (use_gesture_state & VR_USE_GESTURE_OFF_HAND) {
					sendButtonAction("+use2", false);
				}
				if (use_gesture_state & VR_USE_GESTURE_WEAPON_HAND) {
					sendButtonAction("+use", false);
				}
				use_gesture_state = 0;
			}

            sendButtonAction("+speed", running ? 1 : 0);
		}
	}

	//Save state
	rightTrackedRemoteState_old = rightTrackedRemoteState_new;
	leftTrackedRemoteState_old = leftTrackedRemoteState_new;
}