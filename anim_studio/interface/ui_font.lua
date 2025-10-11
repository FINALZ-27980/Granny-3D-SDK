
-- Handy constants (must match ui_font.h)
InvalidFont   = -1

FontCentered   = 1 -- (1 << 0) fuck you lua for not have shift
FontMultiline  = 2 -- (1 << 1)
FontBreakWords = 4 -- (1 << 2)

UIFont = {
   Get = UIFont_Get
}
