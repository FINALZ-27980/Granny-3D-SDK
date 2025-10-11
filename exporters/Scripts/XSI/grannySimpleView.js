/* <help>\tGrannySimpleView ()
   <help>
   <help>exports the current scene with as much of the current
   <help>view settings preserved as possible, and loads the file
   <help>in the Granny Viewer.
   by: dmoore 2007/08/07
   version 0.1

   0.1:     initial revision
*/

function grannySimpleView()
{
    var SettingsFile = "c:/temp/gr_sets.sav";
    GrannySaveSettings(SettingsFile);

    GrannySetGlobalValue("include reference in export", true, "Animations");
    GrannySetGlobalValue("include reference in export", true, "Materials");
    GrannySetGlobalValue("include reference in export", true, "Meshes");
    GrannySetGlobalValue("include reference in export", true, "Models");
    GrannySetGlobalValue("include reference in export", true, "Textures");

    var ModelFile = "c:/temp/gr_sets.sav";
    GrannyExport(ModelFile, false, false);
    GrannyLoadSettings(SettingsFile);

    // Fire up the granny viewer with the appropriate file
    XSIUtils.LaunchProcess("gr2_viewer -autoplay " + ModelFile, false);
}

