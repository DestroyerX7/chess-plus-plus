"use client";

import useChessEngine from "@/hooks/useChessEngine";
import Image from "next/image";
import { useState } from "react";

const yo: Record<string, string> = {
  P: "/chess-piece-images/wp.png",
  N: "/chess-piece-images/wn.png",
  B: "/chess-piece-images/wb.png",
  R: "/chess-piece-images/wr.png",
  Q: "/chess-piece-images/wq.png",
  K: "/chess-piece-images/wk.png",
  p: "/chess-piece-images/bp.png",
  n: "/chess-piece-images/bn.png",
  b: "/chess-piece-images/bb.png",
  r: "/chess-piece-images/br.png",
  q: "/chess-piece-images/bq.png",
  k: "/chess-piece-images/bk.png",
};

function chunk<T>(arr: T[], size: number): T[][] {
  return Array.from({ length: Math.ceil(arr.length / size) }, (_, i) =>
    arr.slice(i * size, i * size + size),
  );
}

export default function Chessboard() {
  const { chessEngine, fen, legalMoves, clearLegalMoves, evaluation } =
    useChessEngine();
  const [fromSquare, setFromSquare] = useState<number | null>(null);

  const playMove = (toSquare: number) => {
    chessEngine?.postMessage({ type: "playMove", fromSquare, toSquare });

    setFromSquare(null);
  };

  return (
    <div>
      <p>{evaluation}</p>

      <button
        onClick={() => {
          chessEngine?.postMessage({ type: "playBestMove" });
          console.log(fen)
        }}
      >
        Play Best
      </button>

      <div className="grid grid-cols-8 max-h-full aspect-square">
        {chunk(fen.split(""), 8).map((row, i) => {
          return row.map((character, j) => (
            <div
              key={`${i}-${j}`}
              className={
                "relative aspect-square " +
                ((i + j) % 2 === 0 ? "bg-[#EBECD0]" : "bg-[#739552]")
              }
              onClick={() => {
                if (
                  character >= "A" &&
                  character <= "Z" &&
                  fromSquare !== (7 - i) * 8 + j
                ) {
                  chessEngine?.postMessage({
                    type: "getLegalMoves",
                    fromSquare: (7 - i) * 8 + j,
                  });

                  setFromSquare((7 - i) * 8 + j);
                } else if (legalMoves.includes((7 - i) * 8 + j)) {
                  playMove((7 - i) * 8 + j);
                } else {
                  setFromSquare(null);
                  clearLegalMoves();
                }
              }}
            >
              {character !== "0" && (
                <Image
                  src={yo[character]}
                  alt={character}
                  fill
                  priority
                  sizes="33vw"
                />
              )}

              {legalMoves.includes((7 - i) * 8 + j) && (
                <div className="w-full h-full flex items-center justify-center">
                  <div className="w-1/2 aspect-square rounded-full bg-white/75 shadow-xl" />
                </div>
              )}
            </div>
          ));
        })}
      </div>
    </div>
  );
}
