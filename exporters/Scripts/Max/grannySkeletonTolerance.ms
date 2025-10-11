fn NodeDist obj0 obj1 =
(
    return length (obj0.pos = obj1.pos)
)
-- {
--     float $xDif = (`getAttr ($obj0 + ".translateX")` - `getAttr ($obj1 + ".translateX")`);
--     float $yDif = (`getAttr ($obj0 + ".translateY")` - `getAttr ($obj1 + ".translateY")`);
--     float $zDif = (`getAttr ($obj0 + ".translateZ")` - `getAttr ($obj1 + ".translateZ")`);
--
--     return sqrt(($xDif * $xDif) + ($yDif * $yDif) + ($zDif * $zDif));
-- }

fn ToleranceRecurse bone EETolerance DistLowerBound AngLowerBound =
(
    maxHere = DistLowerBound
    for obj in bone.children do
    (
        thisDist   = NodeDist bone obj
        recurseVal = ToleranceRecurse obj EETolerance DistLowerBound AngLowerBound

        thisVal = thisDist + recurseVal
        if (thisVal > maxHere) do
        (
            maxHere = thisVal
        )
    )

    AngTol = AngLowerBound;
    if (maxHere > EETolerance) do
    (
        AngTol = radToDeg(asin(EETolerance / maxHere))
        if (AngTol < AngLowerBound) do
        (
            AngTol = AngLowerBound
        )
    )

    GrannySetValue "Position curve tolerance" EETolerance boneanim:bone;
    GrannySetValue "Orientation curve tolerance" AngTol boneanim:bone;
    return maxHere
)

fn HeuristicTolerance bone EETolerance DistLowerBound AngLowerBound =
(
    ToleranceRecurse bone EETolerance DistLowerBound AngLowerBound
)
