// 可視のポストエフェクト対象を 1 として出力する。位置は通常描画と同じ VS で計算する。
float4 main() : SV_TARGET
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
