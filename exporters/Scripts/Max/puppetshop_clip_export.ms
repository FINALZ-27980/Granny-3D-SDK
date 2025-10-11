-- Simple script that exports all of the puppetshop clips associated with a puppetNode
--
fn PuppetAnimsExport puppetNode DestDir FilePrefix =
(
    startAnim = puppet.getActiveAnimationName puppetNode

    for i = 1 to puppet.getAnimCount puppetNode do
    (
        info = puppet.getAnimInfo puppetNode i
        name = info[2]
        format "%\n" name

        puppet.setActiveAnimation puppetNode i
        full_path = DestDir + FilePrefix + "_" + name + ".gr2"
        GrannyExport full_path
        DOSCommand ("preprocessor RenameElement \"" + full_path + "\" -output \"" + full_path + "\" -rename Animations[0] -newname \"" + name + "\"")
        DOSCommand ("preprocessor Compress \"" + full_path + "\" -output \"" + full_path + "\"")
    )

    puppet.setActiveAnimationByName puppetNode startAnim
)
