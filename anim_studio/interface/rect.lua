-- Some handy rect functions

function MakeRect(in_x, in_y, in_w, in_h)
   return { x = in_x, y = in_y, w = in_w, h = in_h }
end

function RectFromPoints(pt1, pt2)
   return { x = math.min(pt1.x, pt2.x),
            y = math.min(pt1.y, pt2.y),
            w = math.abs(pt1.x - pt2.x),
            h = math.abs(pt1.y - pt2.y) }
end

function CopyRect(Rect)
   return { x = Rect.x,
            y = Rect.y,
            w = Rect.w,
            h = Rect.h }
end

function ClipRect(RectOne, RectTwo)
   local xm1 = RectOne.x + RectOne.w
   local ym1 = RectOne.y + RectOne.h

   local xm2 = RectTwo.x + RectTwo.w
   local ym2 = RectTwo.y + RectTwo.h

   local NewX, NewY, NewW, NewH
   if (RectOne.x > RectTwo.x) then NewX = RectOne.x else NewX = RectTwo.x end
   if (RectOne.y > RectTwo.y) then NewY = RectOne.y else NewY = RectTwo.y end
   if (xm1 < xm2) then NewW = xm1 else NewW = xm2 end
   if (ym1 < ym2) then NewH = ym1 else NewH = ym2 end

   local NewRect = { x = NewX,
                     y = NewY,
                     w = NewW - NewX,
                     h = NewH - NewY }
   if (NewRect.w < 0) then NewRect.w = 0 end
   if (NewRect.h < 0) then NewRect.h = 0 end

   return NewRect
end

function UnionRect(RectOne, RectTwo)
   local xm1 = math.min(RectOne.x, RectTwo.x)
   local ym1 = math.min(RectOne.y, RectTwo.y)
   local xm2 = math.max(RectOne.x + RectOne.w, RectTwo.x + RectTwo.w)
   local ym2 = math.max(RectOne.y + RectOne.h, RectTwo.y + RectTwo.h)

   return MakeRect(xm1, ym1, xm2 - xm1, ym2 - ym1)
end

function RectsOverlap(RectOne, RectTwo)
   local x  = RectOne.x
   local y  = RectOne.y
   local xw = RectOne.x + RectOne.w
   local yh = RectOne.y + RectOne.h

   local px  = RectTwo.x
   local py  = RectTwo.y
   local pxw = RectTwo.x + RectTwo.w
   local pyh = RectTwo.y + RectTwo.h

   return not (((px > xw) or (pxw < x)) or ((py > yh) or (pyh < y)))
end

function RectContainsPoint(Rect, Point)
   return (Point.x >= Rect.x and
           Point.y >= Rect.y and
           Point.x < Rect.x + Rect.w and
           Point.y < Rect.y + Rect.h)
end

function IsRect(Maybe)
   return (Maybe ~= nil and (Maybe.x ~= nil and
                             Maybe.y ~= nil and
                             Maybe.w ~= nil and
                             Maybe.h ~= nil))
end

function DumpRect(Rect, Prefix)
   print(Prefix, Rect.x, Rect.y, Rect.w, Rect.h)
end
