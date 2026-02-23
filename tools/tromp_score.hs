-- Tromp-Taylor area scoring.
-- Core scoring logic by John Tromp (https://tromp.github.io/go.html).
-- CLI wrapper reads a board from stdin and prints scores.
--
-- Input format:
--   First line: board size (e.g. "9")
--   Next N lines: row of '.', 'B', 'W' separated by spaces
--   Then: komi as a decimal (e.g. "7.5")
--
-- Output format:
--   B=<black_score> W=<white_score>

module Main where
import Data.List (nub)

-- Board representation (parameterized size)
type Point = (Int,Int)

data Player = Black | White deriving (Eq, Show)
data Color = Empty | Stone Player deriving (Eq, Show)
type Position = Point -> Color

-- Tromp's neighbours function, parameterized by board size.
neighbours :: Int -> Point -> [Point]
neighbours sz (x,y) = [(x,y1) | y1 <- neighbours1 sz y] ++
                       [(x1,y) | x1 <- neighbours1 sz x]
  where neighbours1 s z = [pred z | z /= 1] ++ [succ z | z /= s]

-- Tromp's string (connected group) function.
string :: Int -> Position -> Point -> [Point]
string sz pos point = join' (expand [[point],[]]) where
  expand l@(curr:prev:_) = if null next then l else expand (next:l) where
    next = nub [nbr | nbr <- concatMap (neighbours sz) curr,
                       pos nbr == pos point] \\ prev
  expand l = l
  join' = concat
  (\\) xs ys = filter (`notElem` ys) xs

-- Tromp's score function (article 9 of Tromp-Taylor rules):
-- "A player's score is the number of points of her color, plus the number
-- of empty points that reach only her color."
score :: Int -> Position -> Player -> Int
score sz pos player = sum $ map scorepoint pts where
  pts = [(x,y) | x <- [1..sz], y <- [1..sz]]
  scorepoint pt = case pos pt of
    Stone p | p == player -> 1
            | otherwise   -> 0
    Empty   | owners == [player] -> 1
            | otherwise          -> 0
      where owners = nub [p | (Stone p) <- map pos (concatMap (neighbours sz)
                                                    (string sz pos pt))]

-- Parse input
parseColor :: Char -> Color
parseColor 'B' = Stone Black
parseColor 'W' = Stone White
parseColor _   = Empty

main :: IO ()
main = do
  szLine <- getLine
  let sz = read szLine :: Int
  rows <- mapM (\_ -> do
    line <- getLine
    let chars = filter (\c -> c == '.' || c == 'B' || c == 'W') line
    return chars
    ) [1..sz]
  komiLine <- getLine
  let komi = read komiLine :: Double
  -- Build position function from parsed grid.
  -- Row 1 is the first line of input (top of board).
  let grid = concat rows
      pos (x,y) = parseColor (grid !! ((y-1) * sz + (x-1)))
      bScore = fromIntegral (score sz pos Black)
      wScore = fromIntegral (score sz pos White) + komi
  putStrLn $ "B=" ++ show bScore ++ " W=" ++ show wScore
