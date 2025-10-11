require "math"

function DistPoint(A, B)
   return math.sqrt((A.x - B.x) * (A.x - B.x) +
                    (A.y - B.y) * (A.y - B.y))
end

function MakePoint(inx, iny)
   return { x = inx, y = iny }
end

function CopyPoint(pt)
   return { x = pt.x, y = pt.y }
end

function SubPoint(A, B)
   return { x = A.x - B.x,
            y = A.y - B.y }
end

function DotPoint(A, B)
   return ((A.x * B.x) + (A.y * B.y))
end
