"use client";

import { useEffect, useRef, useState } from "react";

export default function useChessEngine() {
  const workerRef = useRef<Worker | null>(null);
  const [ready, setReady] = useState(false);
  const [fen, setFen] = useState(
    "rnbqkbnrpppppppp00000000000000000000000000000000PPPPPPPPRNBQKBNR",
  );
  const [legalMoves, setLegalMoves] = useState<number[]>([]);

  useEffect(() => {
    workerRef.current = new Worker(
      new URL("@/workers/chess-engine.worker.ts", import.meta.url),
    );

    workerRef.current.onmessage = (e) => {
      const { type } = e.data;

      if (type === "setPosition") {
        setReady(true);
      } else if (type === "playBestMove") {
        setFen(e.data.fen);
      } else if (type === "getLegalMoves") {
        setLegalMoves(e.data.legalMoves);
      } else if (type === "playMove") {
        setFen(e.data.fen);
        setLegalMoves([]);

        workerRef.current?.postMessage({ type: "playBestMove" });
      }
    };

    workerRef.current.postMessage({ type: "setPosition", fen, color: 0 });

    return () => workerRef.current?.terminate();
  }, []);

  return {
    ready,
    chessEngine: workerRef.current,
    fen,
    legalMoves,
    clearLegalMoves: () => setLegalMoves([]),
  };
}
