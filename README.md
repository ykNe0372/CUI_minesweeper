## 概要
CUI上でマインスイーパーを遊ぶことができます。Windows / Mac 両対応です。

通常のマインスイーパーに加えて、マインスイーパーに更に一つルールを追加した特殊モードを遊ぶことも可能です。
現在、以下の2種類のルールを追加することが出来ます。
- **Triplet**：地雷が縦・横・斜めに三連続で並ばない
- **Cross**：地雷の探知範囲が周囲８マスから上下左右2マスずつの十字形になる 

## デモプレイ
↓Windowsでの動作

<video src="./images-videos/minesweeper_win.mp4" controls width="100%"></video>

↓Macでの動作

<video src="./images-videos/minesweeper_mac.mov" controls width="100%"></video>

## 操作方法
- 矢印キー：カーソルの移動
- Spaceキー：マスを開ける
- Fキー：旗を立てる・旗を外す

※Ctrl+Cキー を押すことで、ゲームを強制終了することができます。

## 実行方法
`minesweeper.cpp` を実行することでマインスイーパーを遊ぶことができます（C++ の実行環境が必要です）。