--------------------------------------------------------------------------
-- Granny Casting UI for max
-------------------------------------------------------------------------
-- version 0.9
-- theo 3/31/04
-- pops up a grannyCast dialog window which allows easy setting of common casting options
-- call with grannyCastOptions()


-- utility functions

-- filter that makes sure the same object isn't in source and target lists
-- DISABLED because max doesn't seem to initialize correctly
-- TODO: reinstate
fn grannyCastSourceObjects obj =
(
    return (classof Obj == Editable_mesh or classOf obj == Editable_poly)
)

-- for forcing textures to be a power of two
fn nearestPowerTwo num =
(
    test = (num as integer) + 1
    remainder = 1
    while (remainder != 0) do
    (
        test = test - 1
        remainder = bit.and test (test - 1)
    )

    return test
)

-- returns object reference from object name
fn objFromName name =
(
    obj =  stringstream("")
    format "$%" name to: obj
    seek obj 0
    item = execute (obj)
    return item
)


--------------------------------------------------------------------------
-- Rollout definitions for the window
-------------------------------------------------------------------------



--------------------------------------------------------------------------
-- the source (high res) objects rollout
-------------------------------------------------------------------------

rollout grannyCastSource "Source Objects" width:480 height:112
(

    -- this stores the source objects as an array of string names
    persistent global grannyCast_fromMeshArray

    listbox grannyCastSrcItems "" pos:[192,10] width:278 height:7
    pickbutton grannyCastAddSrcItem "Add scene object to source list" \
        pos:[12,46] width:168 height:26 message:"pick objects to add to Source list" \
        filter:grannyCastSourceObjects toolTip:"Adds objects to the casting Source list"

    button grannyCastRemoveSrcItem "Remove object from list" pos:[12,79] width:168 height:26
    label lbl1 "Use buttons to add or remove source objects from list at right" pos:[15,11] width:165 height:29

    on grannyCastSource open do
    (
        if grannyCast_fromMeshArray != undefined do
        (
            grannyCastSrcItems.items = grannyCast_fromMeshArray
        )
    )

    on grannyCastSource close do
    (
        grannyCast_fromMeshArray = grannyCastSrcItems.items
    )

    on grannyCastSrcItems doubleClicked sel do
    (
        if (Querybox ("Remove item `" + grannyCastSrcItems.items[sel] + "` from Source list?"))  do
        (
            list = grannyCastSrcItems.items
            deleteItem list sel
            grannyCastSrcItems.items = list
        )
    )

    on grannyCastAddSrcItem picked obj do
    (
        list = grannyCastSrcItems.items
        append list obj.name
        sort list
        grannyCastSrcItems.items = list
    )

    on grannyCastRemoveSrcItem pressed do
    (
        list = grannyCastSrcItems.items
        SelectedItem = grannyCastSrcItems.selection
        if (SelectedItem > 0) do
        (
            deleteItem list selectedItem
            grannyCastSrcItems.items = list
            grannyCastSrcItems.selection = 0
        )
    )

    --- the reporting method that gets the option flags from this as a string
    fn getCommand =
    (
        cmd = "" as stringstream
        for item in grannyCastSrcItems.items do
        (
            format " fromMesh:$'%'" item to: cmd
        )

        seek cmd 0
        return cmd as string
    )
)


--------------------------------------------------------------------------
-- the target objects rollout
-------------------------------------------------------------------------
rollout grannyCastTarget "Target Objects" width:480 height:128
(

    -- for save/restore of options
    persistent global grannyCast_toMeshArray

    listbox grannyCastTgtItems "" pos:[192,10] width:278 height:7
    pickbutton grannyCastAddTgtItem "Add scene object to Target list" \
        pos:[12,46] width:168 height:26 message:"pick objects to add to Target list" \
        filter:grannyCastSourceObjects toolTip:"Adds objects to the casting Target list"

    button grannyCastRemoveTgtItem "Remove object from list" pos:[12,79] width:168 height:26
    label lbl1 "Use buttons to add or remove Target objects from list at right" pos:[15,11] width:165 height:29

    on grannyCastTarget open do
    (
        if grannyCast_toMeshArray != undefined do
        (
            grannyCastTgtItems.items = grannyCast_toMeshArray
        )
    )

    on grannyCastTarget close do
    (
        grannyCast_toMeshArray  = grannyCastTgtItems.items
    )

    on grannyCastTgtItems doubleClicked sel do
    (
        if (Querybox ("Remove item `" + grannyCastTgtItems.items[sel] + "` from Target list?"))  do
        (
            list = grannyCastTgtItems.items
            deleteItem list sel
            grannyCastTgtItems.items = list
        )
    )

    on grannyCastAddTgtItem picked obj do
    (
        list = grannyCastTgtItems.items
        append list obj.name
        sort list
        grannyCastTgtItems.items = list
    )

    on grannyCastRemoveTgtItem pressed do
    (
        list = grannyCastTgtItems.items
        SelectedItem = grannyCastTgtItems.selection
        if (SelectedItem > 0) do
        (
            deleteItem list selectedItem
            grannyCastTgtItems.items = list
            grannyCastTgtItems.selection = 0
        )
    )

    fn getCommand =
    (
        cmd = "" as stringstream
        for item in grannyCastTgtItems.items do
        (
            format " toMesh:$'%'" item to: cmd
        )

        seek cmd 0
        return cmd as string
    )
)


--------------------------------------------------------------------------
-- the file options rollout
-------------------------------------------------------------------------
rollout GrannyCastFileOptions "Granny Output Options" width:480 height:170
(
    -- for save/restore of options
    persistent global grannyCast_suffix
    persistent global grannyCast_extension
    persistent global grannyCast_alpha
    persistent global grannyCast_overWrite
    persistent global grannyCast_uPixels
    persistent global grannyCast_vPixels
    persistent global grannyCast_autoClose
    persistent global grannyCast_forceTwo
    persistent global grannyCast_forceSquare

    edittext suffix "File name suffix:" pos:[13,12] width:440 height:24 \
        text:(if grannyCast_suffix == undefined then "_grannyCast" else grannyCast_suffix)
    GroupBox fileTypes "Output file types:" pos:[10,48] width:220 height:90

    radiobuttons colorDepth "Color Depth:" pos:[125,68] width:88 height:46 \
        labels:#("RGB (24-bit)", "RGBA (32-bit)") columns:1 \
        default:(if grannyCast_alpha == undefined then 2 else grannyCast_alpha)

    radiobuttons extension "File type:" pos:[22,68] width:67 height:62 enabled:true \
        labels:#("BMP files", "TGA files", "DDS files") columns:1 \
        default:(if grannyCast_extension == undefined then 1 else grannyCast_extension)

    checkbox overWrite "Overwriting existing files without asking" pos:[12,143] width:221 height:21 \
        checked:(if grannyCast_overWrite == undefined then true else grannyCast_overWrite)

    GroupBox TextureSize "Output texture size:" pos:[250,48] width:220 height:90
    spinner uPixels "U:" pos:[287,72] width:53 height:16 \
        type:#integer scale:64 \
        range:[64,4096,(if grannyCast_uPixels == undefined then 512 else grannyCast_Upixels)]

    checkbox forceSquare "force square" pos:[292,98] width:96 height:15 \
        checked:(if grannyCast_forceSquare == undefined then true else grannyCast_forceSquare)

    spinner vPixels "V:" pos:[379,72] width:53 height:16 enabled:false \
        type:#integer scale:64 \
        range:[64,4096,(if grannyCast_vPixels == undefined then 512 else grannyCast_vpixels)]

    checkbox autoClose "Auto-close casting window" pos:[251,144] width:209 height:21 \
        checked:(if grannyCast_autoClose == undefined then true else grannyCast_autoClose)

    checkbox forceTwo "force powers of two" pos:[292,119] width:151 height:15 \
        checked:(if grannyCast_forceTwo == undefined then true else grannyCast_forceTwo)

    on grannyCastFileOptions close do
    (
        grannyCast_suffix = suffix.text
        grannyCast_alpha = colorDepth.state
        grannyCast_extension = extension.state
        grannyCast_overWrite = overWrite.checked
        grannyCast_uPixels = uPixels.value
        grannyCast_vPixels = vPixels.value
        grannyCast_autoClose = autoClose.checked
        grannyCast_forceTwo = forceTwo.checked
        grannyCast_forceSquare = forceSquare.checked
    )

    fn getCommand =
    (
        cmd = "" as stringstream
        format " uPixels:%" uPixels.value to:cmd
        vPixelValue = if (forceSquare.checked) then vPixels.value else uPixels.value
        format " vPixels:%" vPixelValue to:cmd
        format " extension:\"%\"" (#("BMP", "TGA", "DDS")[extension.state]) to:cmd
        format " overwrite:%" overwrite.state to:cmd
        format " alpha:%" (colorDepth.state == 2) to: cmd
        format " autoclose:%" autoClose.state to:cmd
        format " suffix:\"%\"" suffix.text to:cmd
        seek cmd 0
        return cmd as string
    )

    on suffix entered text do
    (
        text = trimRight text
        suffix.text = text
    )

    on uPixels changed val do
    (
        if forceTwo.checked do uPixels.value = nearestPowerTwo (val)
        if (vPixels.enabled == false) do vPixels.value = uPixels.value
    )

    on forceSquare changed state do
    (
        vPixels.enabled = (not state)
        if (state) do vPixels.value = uPixels.value
    )

    on vPixels changed val do
    (
        if forceTwo.checked do vPixels.value = nearestPowerTwo (val)
    )
)

--------------------------------------------------------------------------
-- the casting options rollout
-------------------------------------------------------------------------
rollout grannyCastingOptions "Casting Options" width:480 height:148 rolledUp:true
(
    -- for save/restore of options
    persistent global grannyCast_rayNormal
    persistent global grannyCast_fillUncast
    persistent global grannyCast_samples
    persistent global grannyCast_ceil
    persistent global grannyCast_floor
    persistent global grannyCast_useLimits
    persistent global grannyCast_castType
    persistent global grannyCast_allowBackfaces

    -- controls
    radiobuttons rayNormal "" pos:[312,31] width:148 height:32 \
        labels:#("Use low-res model normals", "Show errors with gray") columns:1 \
        default:(if grannyCast_rayNormal == undefined then 1 else grannyCast_rayNormal)

    radiobuttons fillUncast "" pos:[312,96] width:109 height:32 \
        labels:#("Nearest cast color", "Leave empty") columns:1 \
        default:(if grannyCast_fillUncast == undefined then 1 else grannyCast_fillUncast)

    spinner sampling "" pos:[218,28] width:72 height:16 type:#integer scale:1 columns:1\
        range:[1,64,(if grannyCast_samples == undefined then 1 else grannyCast_samples)]

    checkbox useLimits "use limits" pos:[184,71] width:81 height:21 \
        checked:(if grannyCast_useLimits == undefined then false else grannyCast_useLimits)

    spinner ceil "max:" pos:[221,93] width:72 height:16  type:#worldunits\
        range:[-1000,1000,(if grannyCast_ceil == undefined then -10 else grannyCast_ceil)]\
        enabled:useLimits.checked

    spinner floor "min:" pos:[221,114] width:72 height:16  type:#worldunits\
        range:[-1000,1000,(if grannyCast_floor == undefined then -10 else grannyCast_floor)]\
        enabled:useLimits.checked

    checkbox allowBackface "allow hits on backfaces" pos:[15,105] width:147 height:20\
        checked:(if grannyCast_allowBackfaces == undefined then false else grannyCast_allowBackfaces)

    GroupBox grp41 "Fill empty texels with:" pos:[305,78] width:160 height:60
    GroupBox grp42 "GroupBox" pos:[464,99] width:0 height:0
    GroupBox grp43 "When rays find no target" pos:[305,13] width:160 height:60

    radiobuttons castType "" pos:[15,32] width:139 height:48 \
        labels:#("Use closest point", "Use closest point in front", "Use furthest point") columns:1\
        default:(if grannyCast_castType == undefined then 1 else grannyCast_castType)

    GroupBox grp48 "GroupBox" pos:[359,20] width:0 height:0
    GroupBox grp49 "Raycast hits:" pos:[8,13] width:160 height:125

    GroupBox grp50 "Limit search:" pos:[174,53] width:125 height:85
    GroupBox grp71 "Rays per texel:" pos:[174,13] width:125 height:36

    -- save options for reuse
    on grannyCastingOptions close do
    (
        grannyCast_rayNormal = rayNormal.state
        grannyCast_fillUncast = fillUncast.state
        grannyCast_samples = sampling.value
        grannyCast_ceil = ceil.value
        grannyCast_floor = floor.value
        grannyCast_useLimits = useLimits.checked
        grannyCast_castType = castType.state
        grannyCast_allowBackfaces = allowBackface.checked
     )

    -- the reporting method
    fn getCommand =
    (
        cmd = "" as stringstream
        format " useRayNormalForMisses:% allowFallback:%" (RayNormal.state == 1) (rayNormal.state == 1) to:cmd
        format " fillUncastPixels:%" (fillUncast.state == 1)  to:cmd
        format " supersampling:%" sampling.value to:cmd
        case (castType.state) of
        (
            1: (format " useAbsoluteValue:false pickClosest:true" to:cmd)
            2: (format " useAbsoluteValue:true pickClosest:true" to:cmd)
            3: (format " useAbsoluteValue:true pickClosest:false" to:cmd)
        )
        format " allowFrontFacing:%" allowBackface.state to:cmd
        if (useLimits.checked) do format " floorDistance:% ceilingDistance:%" floor.value ceil.value to: cmd
        seek cmd 0
        return cmd as string
    )

    on useLimits changed state do
    (
        floor.enabled = state
        ceil.enabled = state
    )
)

--------------------------------------------------------------------------
-- the normal mapping options rollout
-------------------------------------------------------------------------
rollout grannyNormalMappingOptions "Normal Mapping Options" width:480 height:123 rolledUp:true
(
    -- for save/restore
    persistent global grannyCast_bumpHeight
    persistent global grannyCast_tangTolerance
    persistent global grannyCast_outputNormals
    persistent global grannyCast_mirror
    persistent global grannyCast_mirrorPoint
    persistent global grannyCast_uMult
    persistent global grannyCast_vMult
    persistent global grannyCast_nMult

    -- controls
    spinner bumpHeight "Bump map scale:" pos:[79,93] width:78 height:16\
        range:[0,100,(if grannyCast_bumpHeight == undefined then 1 else grannyCast_bumpHeight)]

    radiobuttons outputNormals "Output normal data as:" pos:[11,14] width:157 height:62 \
        labels:#("Tangent space normal maps", "Object space normal maps", "Bump maps") columns:1\
        default:(if grannyCast_outputNormals == undefined then 1 else grannyCast_outputNormals )

    dropdownList Mirror "Mirroring:" pos:[189,34] width:112 height:40 enabled:(outputNormals.state == 2) \
        items:#("None", "+X to -X", "-X to +X", "+Y to -Y", "-Y to +Y", "+Z to -Z", "-Z to +Z")\
        selection:(if grannyCast_mirror == undefined then 1 else grannyCast_mirror)

    spinner mirrorPoint "About point:" pos:[230,83] width:76 height:16 type:#worldunits\
        enabled:(mirror.selection > 1)\
        range:[-8192,8192,(if grannyCast_mirrorPoint == undefined then 0 else grannyCast_mirrorPoint)]

    GroupBox objectOptions "Object space options:" pos:[178,10] width:145 height:122 enabled:(outputNormals.state == 2)
    GroupBox tangentOptions "Tangent space orientation:" pos:[330,11] width:145 height:122 enabled:(outputNormals.state == 1)

    spinner uMult "U:" pos:[372,39] width:60 height:16 \
        range:[-10,10,(if grannyCast_uMult == undefined then 1 else grannyCast_uMult)] \
        align:#right enabled:(outputNormals.state == 1)

    spinner vmult "V:" pos:[372,61] width:60 height:16 \
        range:[-10,10,(if grannyCast_vMult == undefined then 1 else grannyCast_vMult)] \
        align:#right enabled:(outputNormals.state == 1)

    spinner nMult "Normal:" pos:[372,83] width:60 height:16 \
        range:[-10,10,(if grannyCast_nMult == undefined then 1 else grannyCast_nMult)] \
        align:#right enabled:(outputNormals.state == 1)

    spinner mergeTol "Merge Tol:" pos:[372,105] width:78 height:16 \
        range:[0, 180, (if grannyCast_tangTolerance == undefined then 75 else grannyCast_tangTolerance)] \
        align:#right enabled:(outputNormals.state == 1)

    -- save the options
    on GrannyNormalMappingOptions close do
    (
        grannyCast_bumpHeight = bumpHeight.value
        grannyCast_outputNormals = outputNormals.state
        grannyCast_mirror = mirror.selection
        grannyCast_mirrorPoint = mirrorPoint.value
        grannyCast_uMult = uMult.value
        grannyCast_vMult = vMult.value
        grannyCast_nMult = nMult.value
        grannyCast_tangTolerance = mergeTol.value
    )

    on outputNormals changed stat do
    (
        objectOptions.enabled = (stat == 2)
        mirrorPoint.enabled = (stat == 2)
        mirror.enabled = (stat == 2)
        tangentOptions.enabled = (stat == 1)
        uMult.enabled = (stat == 1)
        vMult.enabled = (stat == 1)
        nMult.enabled = (stat ==1)
    )

    -- reporting method
    -- this one is a little tricky since it's conditional
    -- for advanced combinations of options use the commandline
    -- directly
    fn getCommand =
    (
        cmd = "" as stringstream
        format " bumpHeight:%" bumpHeight.value to:cmd
        format " outputFormat:\"%\"" #("tangent","object","bump")[outputNormals.state] to:cmd
        if (outputNormals.state == 2) do case Mirror.selection of
        (
            1: ()
            2: format " mirrorX:% mirrorReverse:false" mirrorPoint.value to:cmd
            3: format " mirrorX:% mirrorReverse:true" mirrorPoint.value to:cmd
            4: format " mirrorY:% mirrorReverse:false" mirrorPoint.value to:cmd
            5: format " mirrorY:% mirrorReverse:true" mirrorPoint.value to:cmd
            6: format " mirrorZ:% mirrorReverse:false" mirrorPoint.value to:cmd
            7: format " mirrorZ:% mirrorReverse:true" mirrorPoint.value to:cmd
        )
        if (outputNormals.state == 1) do
        (
            format " uMultiplier:% vMultiplier:% nMultiplier:% tangentMergeTolerance:%" \
                uMult.value vMult.value nMult.value mergeTol.value \
                to:cmd
        )

        seek cmd 0
        return cmd as string
    )
)

--------------------------------------------------------------------------
-- the action rollout with buttons to cast or save command strings
-------------------------------------------------------------------------

rollout grannyCaster "Cast" width:480 height:112
(
    button cast "Cast" pos:[260,66] width:200 height:33
    button saveMacro "Save options as macro" pos:[20,66] width:200 height:33

    -- assemble and perform the casting string

    -- assemble the command and save it to the listener
    -- drag the command line to toolbar to create a macro button

    button Help "Help" pos:[395,12] width:62 height:33
    label lbl1 "\"Cast\" starts the casting process.   \"Save options as macro\" prints a complete grannyCast command to the Listener; execute it manually or drag it to the toolbar to create a macro script." pos:[22,10] width:329 height:45
    on cast pressed do
    (
        command = "GrannyCast"
        command += grannyCastSource.getCommand()
        command += grannyCastTarget.getCommand()
        command += grannyCastFileOptions.getCommand()
        command += grannyCastingOptions.getCommand()
        command += grannyNormalMappingOptions.getCommand()
        command = command as stringstream
        seek command 0
        execute command
    )
    on saveMacro pressed do
    (
        command = "GrannyCast"
        command += grannyCastSource.getCommand()
        command += grannyCastTarget.getCommand()
        command += grannyCastFileOptions.getCommand()
        command += grannyCastingOptions.getCommand()
        command += grannyNormalMappingOptions.getCommand()
        format "%\n" command
    )
    on Help pressed  do
        Shelllaunch  "http://www.radgametools.com/granny/casting.html" ""
)

--------------------------------------------------------------------------
-- Build & display the window
-------------------------------------------------------------------------

fn grannyCastOptions =
(
    grannyCastWindow = newRolloutFloater "Granny Cast Options" 480 640
    addRollout grannyCastSource grannyCastWindow
    addRollout grannyCastTarget grannyCastWindow
    addRollout grannyCastFileOptions grannyCastWindow
    addRollout grannyCastingOptions grannyCastWindow
    addRollout grannyNormalMappingOptions grannyCastWindow
    addRollout grannyCaster grannyCastWindow
)
