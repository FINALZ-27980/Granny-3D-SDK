-- /*
-- <help>\tGrannySimpleView ()
-- <help>
-- <help>exports the current scene with as much of the current
-- <help>view settings preserved as possible, and loads the file
-- <help>in the Granny Viewer.
-- by: dmoore 7/7/2006
-- version 0.1
--
-- 0.1:     initial revision
--
-- */

fn grannySimpleView =
(
   SettingsSave = sysinfo.tempdir + "gr_sets.sav";
   GrannySaveSettings SettingsSave;

   GrannySetValue "include reference in export" true global:"Animations";
   GrannySetValue "include reference in export" true global:"Materials";
   GrannySetValue "include reference in export" true global:"Meshes";
   GrannySetValue "include reference in export" true global:"Models";
   GrannySetValue "include reference in export" true global:"Textures";

   currSel = getCurrentSelection();
   clearSelection();
   for o in objects where o.isHidden == false do selectmore o;

--    $SelPanel = `getPanel -wf`;
--    $SelCamera = `modelPanel -q -cam $SelPanel`;
--       select -add $SelCamera;

   ModelFile = sysinfo.tempdir + "grtemp.gr2";
   GrannyExportSelected ModelFile;

   -- Restore the settings
   clearSelection();
   for o in currSel do selectmore o;

   GrannyLoadSettings SettingsSave;

   -- Fire up the granny viewer with the appropriate file
   shellLaunch "gr2_viewer" ("-autoplay " + ModelFile);
)
