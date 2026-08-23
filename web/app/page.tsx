import Chessboard from "@/components/Chessboard";

export default function Home() {
  return (
    <div>
      <div className="h-screen aspect-square mx-auto">
        <Chessboard />
      </div>
    </div>
  );
}
