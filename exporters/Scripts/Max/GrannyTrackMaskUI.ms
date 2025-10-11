fn GTM_IsMaskableItem item =
(
    return (classof item == BoneGeometry or
            classof item == Dummy or
            classof item == Biped_Object)
)

-- read the current attribute definition into the gui
fn GrannyRebuildTrackList =
(
    global GrannyTrackMaskAttribute
    global GTM_rollout
    trackMaskList = #()

    if GrannyTrackMaskAttribute == undefined do
    (
        -- try to find the attribute in an existing bone...
        allBones = for item in $objects where GTM_IsMaskableItem item == true collect item
        for bone in allBones do
        (
            myAtts = custAttributes.getDefs bone
            if myAtts != undefined do
            (
                for att in myAtts do
                (
                    if (att.name == #GrannyTrackMasks) do
                    (
                        GrannyTrackMaskAttribute = att
                        exit
                    )
                )
            )

            if (GrannyTrackMaskAttribute != undefined) do
                exit
        )
    )

    if GrannyTrackMaskAttribute != undefined do
    (
        pblock = (custAttributes.getDefSource GrannyTrackMaskAttribute) as stringstream

        while (eof pblock == false) do
        (
            nextWord = readToken pblock
            if (nextWord == undefined) do
                exit

            if (findstring nextWord "TrackMask" == 1) then
            (
                newName = ((substring nextWord 10 999) as name)
                if (newName != #_rollout) do
                    append trackMaskList newName
            )
        )
    )

    return trackMaskList
)

-- rebuild the attribute definition
fn GrannyRebuildAttribute =
(
    global GTM_rollout
    MaskList = GTM_rollout.GTM_Tracklist.items

    if (MaskList.count >= 0) then
    (
        global GrannyTrackMaskAttribute
        command = "" as stringstream;
        format "global GrannyTrackMaskAttribute = attributes GrannyTrackMasks" to:command
        if GrannyTrackMaskAttribute != undefined do
        (
            format " redefine:GrannyTrackMaskAttribute" to:command
        )
        format "\n(\n" to:command

        -- the params section
        format "\nparameters main rollout:TrackMask_rollout\n" to:command
        format "\n(\n" to:command

        for mask in MaskList do
        (
            mask = mask as string
            if (mask.count > 1) do format "TrackMask% type:#float ui:spinner_%\n" mask mask to:command
        )
        format "\n)\n" to:command

        -- the params section
        format "\nrollout TrackMask_rollout \"Granny Track Masks\"\n" to:command
        format "\n(\n" to:command
        for mask in MaskList do
        (
            mask = mask as string
            if (mask.count > 1) do  format "Spinner spinner_%\"%: \" type:#float\n" mask mask to:command
        )
        format "\n)\n)" to:command

        seek command 0;
        execute command
    )
)

-- delete the mask selected in the gui
fn GrannyDeleteTrackMask =
(
    global GTM_rollout
    if (GTM_rollout.GTM_tracklist.selection > 0) do
    (
        itemsArray = GTM_rollout.GTM_tracklist.items
        DeleteItem itemsArray GTM_rollout.GTM_tracklist.selection
        GTM_rollout.GTM_tracklist.items = itemsArray
        GrannyRebuildAttribute ()
    )
)


-- rename selected mask to string "newItem"
fn GrannyRenameTrackMask newItem =
(
    global GTM_rollout
    if (GTM_rollout.GTM_tracklist.selection > 0) do
    (
        allBones = for item in $objects where GTM_IsMaskableItem item == true collect item.name
        iterate = 1
        oldValue = #()

        -- cache old values
        oldAttrib = (".TrackMask" + GTM_rollout.GTM_tracklist.items[GTM_rollout.GTM_tracklist.selection] )
        for bone in allBones do
        (
            try
            (
                oldValue[iterate] = execute ("$'" + bone + "'" + oldAttrib )
            )
            catch
            (
                oldValue[iterate] = 0
            )
            iterate += 1
        )

        -- debug
        -- print oldValue

        itemsArray = GTM_rollout.GTM_tracklist.items
        DeleteItem itemsArray GTM_rollout.GTM_tracklist.selection
        GTM_rollout.GTM_tracklist.items = itemsArray
        GrannyAddTrackMask newItem;

        -- reapply  old values
        attrib = ".TrackMask" + (GrannyValidateName newItem)
        iterate = 1
        for bone in allBones do
        (
            try
            (
                execute ("$'" + bone + "'" + attrib + "= oldValue[iterate]")
            )
            catch
            (
            )
            iterate +=1
        )
    )
)

-- for making sure name is OK
fn GrannyValidateName text =
(
    local alphaNumeric = "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890_"
    text = trimRight text
    text = trimLeft text
    for x = 1 to text.count do
    (
        if (findstring alphaNumeric text[x] == undefined) do text[x] = "_"
    )
    return text
)

fn GrannyApplyTrackMasks =
(
    global GrannyTrackMaskAttribute
    global GTM_rollout
    allBones = for item in $objects where GTM_IsMaskableItem item == true collect item
    for bone in allBones do
    (
        myAtts = custAttributes.getDefs bone
        if (myAtts == undefined or findItem myAtts GrannyTrackMaskAttribute == 0) do
        (
            custAttributes.add bone GrannyTrackMaskAttribute
        )
    )
)

fn GrannyAddTrackMask newName =
(
    global GTM_rollout
    if newName.count > 0 then
    (
        oldList = GTM_rollout.GTM_tracklist.items
        newItem = GrannyValidateName (newName)
        newItem = newItem as name

        if (findItem oldList newItem == 0) then
        (
            oldList = append oldList newItem
            oldList = sort oldList
            GTM_rollout.GTM_tracklist.items = oldList
            GTM_rollout.GTM_newname.text = "";
            GrannyRebuildAttribute ()
        )
        else
        (
            GrannyApplyTrackMasks()
        )
    )
)

fn GrannySetMaskTo bones mask value =
(
    global GrannyTrackMaskAttribute

    attrib = ".TrackMask" + mask + " = " + (value as string)

    for bone in bones do
    (
        myAtts = custAttributes.getDefs bone

        foundAttribute = false
        if myAtts != undefined then
        (
            for att in myAtts do
            (
                if (att == GrannyTrackMaskAttribute) then
                (
                    execute ("$'" + bone.name + "'" + attrib)
                )
            )
        )
    )
)

fn GrannyGetAttrib bone mask =
(
    attrib = ".TrackMask" + mask
    val = execute ("$'" + bone.name + "'" + attrib)
    return val
)

fn GrannySelectMaskNotEqual mask value =
(
    clearSelection()
    newSel = for item in $objects where (GTM_IsMaskableItem item and (GrannyGetAttrib item mask) != value) collect item
    select newSel
)

fn GrannySelectMaskEqual mask value =
(
    clearSelection()
    newSel = for item in $objects where (GTM_IsMaskableItem item and (GrannyGetAttrib item mask) == value) collect item
    select newSel
)

fn GrannyEnableThings val =
(
    global GTM_rollout
    for item in GTM_rollout.controls do
    (
        if (matchPattern item.name pattern:"GTME_*") then
        (
            item.enabled = val
        )
    )
)

fn GrannyTrackMaskUI =
(
    global GTM_rollout
    rollout GTM_rollout "Track masks" width:260 height:400
    (
        editText GTM_newName "" text:"" pos:[71,6] width:180 height:17
        button   GTM_Add "Add mask" pos:[7,6] width:57 height:17
        listBox  GTM_TrackList pos:[7,26] width:242 height:13 enabled:true selection:0
        button   GTME_Delete "Delete" pos:[17,205] width:100 height:22 enabled:false
        button   GTME_Rename "Rename" pos:[141,205] width:100 height:22 enabled:false

        spinner  GTM_value range:[0,1,0] pos:[81,246] width:75 scale:0.01
        button   GTME_SetAll       "Set all" pos:[17,265] width:100 height:22 enabled:false
        button   GTME_SetSelection "Set selection" pos:[141,265] width:100 height:22 enabled:false


        label    GTM_Label "Selection Helpers" pos:[20,300] align:#center width:220 height:22 style_sunkenedge:true
        button   GTME_SelectZero      "== 0"     pos:[17,325]  width:100 height:22 enabled:false
        button   GTME_SelectOne       "== 1"     pos:[17,350]  width:100 height:22 enabled:false
        button   GTME_SelectValue     "== value" pos:[17,375]  width:100 height:22 enabled:false
        button   GTME_SelectNonZero   "!= 0"     pos:[141,325] width:100 height:22 enabled:false
        button   GTME_SelectNonOne    "!= 1"     pos:[141,350] width:100 height:22 enabled:false
        button   GTME_SelectNonValue  "!= value" pos:[141,375] width:100 height:22 enabled:false

        on GTM_newName entered text do
        (
            GrannyAddTrackMask(text)
        )

        on GTM_add pressed do
        (
            GrannyAddTrackMask(GTM_newName.text)
        )

        on GTME_Delete pressed do
        (
            GrannyDeleteTrackMask()
            if (GTM_TrackList.items.count == 0) then
            (
                GTM_TrackList.selection = 0
                GrannyEnableThings false
            )
            else if (GTM_TrackList.selection == 0) then
            (
                GTM_TrackList.selection = GTM_TrackList.items.count
            )
        )

        on GTME_Close pressed do
        (
            destroyDialog GTM_rollout
        )

        on GTM_TrackList selected item do
        (
            if (item > 0) then
            (
                GrannyEnableThings true
            )
            else
            (
                GrannyEnableThings false
            )
        )

        on GTME_SetAll pressed do
        (
            allBones = for item in $objects where GTM_IsMaskableItem item == true collect item
            GrannySetMaskTo allBones GTM_TrackList.selected GTM_value.value
        )

        on GTME_SetSelection pressed do
        (
            selectedBones = for item in selection where GTM_IsMaskableItem item == true collect item
            GrannySetMaskTo selectedBones GTM_TrackList.selected GTM_value.value
        )

        on GTME_rename pressed do
        (
            rollout GTM_renameWindow "Rename Track Mask" width:300 height:100
            (
                editText newNameField "" pos:[10,13] width:280 height:24
                button rename_OK "OK" pos:[170,60] width:120 height:24
                button rename_cancel "Cancel" pos:[20,60] width:120 height:24

                on rename_OK pressed do
                (
                    GrannyRenameTrackMask newNameField.text
                    destroyDialog GTM_renameWindow
                )
                on rename_cancel pressed do
                (
                    destroyDialog GTM_renameWindow
                )
            )

            createDialog GTM_renameWindow modal:true
        )

        on GTME_SelectZero     pressed do ( GrannySelectMaskEqual    GTM_trackList.selected 0 )
        on GTME_SelectOne      pressed do ( GrannySelectMaskEqual    GTM_trackList.selected 1 )
        on GTME_SelectValue    pressed do ( GrannySelectMaskEqual    GTM_trackList.selected GTM_value.value )
        on GTME_SelectNonZero  pressed do ( GrannySelectMaskNotEqual GTM_trackList.selected 0 )
        on GTME_SelectNonOne   pressed do ( GrannySelectMaskNotEqual GTM_trackList.selected 1 )
        on GTME_SelectNonValue pressed do ( GrannySelectMaskNotEqual GTM_trackList.selected GTM_value.value )
    )

    createDialog GTM_rollout 268 400
    GTM_rollout.GTM_TrackList.items = GrannyRebuildTrackList()
    GrannyApplyTrackMasks()
)

fn GrannyResetAttribute =
(
    global GrannyTrackMaskAttribute
    GrannyTrackMaskAttribute = undefined
)

callbacks.addScript #filePreOpen    "GrannyResetAttribute()"
callbacks.addScript #systemPreNew   "GrannyResetAttribute()"
callbacks.addScript #systemPreReset "GrannyResetAttribute()"

-- Start the UI
GrannyTrackMaskUI();
