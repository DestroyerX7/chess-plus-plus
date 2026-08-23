"use client";

import { useEffect, useRef, useState } from "react";

export default function useChessEngine() {
  const workerRef = useRef<Worker | null>(null);
  const [ready, setReady] = useState(false);
  const [fen, setFen] = useState(
    "rnbqkbnrpppppppp00000000000000000000000000000000PPPPPPPPRNBQKBNR",
  );
  const [legalMoves, setLegalMoves] = useState<number[]>([]);
  const [evaluation, setEvaluation] = useState(0);

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

        workerRef.current?.postMessage({ type: "getEval" });
      } else if (type === "getLegalMoves") {
        setLegalMoves(e.data.legalMoves);
      } else if (type === "playMove") {
        setFen(e.data.fen);
        setLegalMoves([]);

        workerRef.current?.postMessage({ type: "playBestMove" });
      } else if (type === "getEval") {
        setEvaluation(e.data.evaluation);
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
    evaluation,
  };
}
