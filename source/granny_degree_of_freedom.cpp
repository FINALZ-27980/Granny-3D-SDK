// ========================================================================
// $File: //jeffr/granny_29/rt/granny_degree_of_freedom.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_degree_of_freedom.h"

#include "granny_math.h"
#include "granny_transform.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

USING_GRANNY_NAMESPACE;

bool GRANNY
ClipPositionDOFs(real32 *Position, uint32x AllowedDOFs)
{
    if(AllowedDOFs & TranslationDOFs)
    {
        if(!(AllowedDOFs & XTranslation))
        {
            Position[0] = 0;
        }

        if(!(AllowedDOFs & YTranslation))
        {
            Position[1] = 0;
        }

        if(!(AllowedDOFs & ZTranslation))
        {
            Position[2] = 0;
        }

        return(true);
    }
    else
    {
        Position[0] = Position[1] = Position[2] = 0.0f;
        return(false);
    }
}

bool GRANNY
ClipAngularVelocityDOFs(real32 *Velocity, uint32x AllowedDOFs)
{
    if(AllowedDOFs & RotationDOFs)
    {
        if(!(AllowedDOFs & XRotation))
        {
            Velocity[0] = 0;
        }

        if(!(AllowedDOFs & YRotation))
        {
            Velocity[1] = 0;
        }

        if(!(AllowedDOFs & ZRotation))
        {
            Velocity[2] = 0;
        }

        return(true);
    }
    else
    {
        Velocity[0] = Velocity[1] = Velocity[2] = 0.0f;
        return(false);
    }
}

bool GRANNY
ClipOrientationDOFs(real32 *Orientation, uint32x AllowedDOFs)
{
    if(AllowedDOFs & RotationDOFs)
    {
        if(!(AllowedDOFs & XRotation))
        {
            Orientation[0] = 0;
        }

        if(!(AllowedDOFs & YRotation))
        {
            Orientation[1] = 0;
        }

        if(!(AllowedDOFs & ZRotation))
        {
            Orientation[2] = 0;
        }

        Normalize4(Orientation);

        return(true);
    }
    else
    {
        Orientation[0] = Orientation[1] = Orientation[2] = 0.0f;
        Orientation[3] = 1.0f;

        return(false);
    }
}

void GRANNY
ClipTransformDOFs(transform &Result, uint32x AllowedDOFs)
{
    if(!ClipPositionDOFs(Result.Position, AllowedDOFs))
    {
        Result.Flags &= ~HasPosition;
    }

    if(!ClipOrientationDOFs(Result.Orientation, AllowedDOFs))
    {
        Result.Flags &= ~HasOrientation;
    }

    if(AllowedDOFs & ScaleShearDOFs)
    {
        if(!(AllowedDOFs & XScaleShear))
        {
            Result.ScaleShear[0][0] = 1.0f;
            Result.ScaleShear[1][0] = 0.0f;
            Result.ScaleShear[2][0] = 0.0f;
        }

        if(!(AllowedDOFs & YScaleShear))
        {
            Result.ScaleShear[0][1] = 0.0f;
            Result.ScaleShear[1][1] = 1.0f;
            Result.ScaleShear[2][1] = 0.0f;
        }

        if(!(AllowedDOFs & ZScaleShear))
        {
            Result.ScaleShear[0][2] = 0.0f;
            Result.ScaleShear[1][2] = 0.0f;
            Result.ScaleShear[2][2] = 1.0f;
        }
    }
    else
    {
        Result.Flags &= ~HasScaleShear;
        MatrixIdentity3x3(Result.ScaleShear);
    }
}

