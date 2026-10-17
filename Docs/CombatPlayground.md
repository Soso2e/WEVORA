# Combat Playground v1

独立マップ: `/Game/WEVORA/Maps/L_CombatPlayground`。既存の`Lvl_ThirdPerson`と既存Blueprintを保存し直さず、専用プレイヤー／GameModeを複製する。

## Stage A: 反復できるフィールド

- 240m四方の平地、200m四方・高度60mの試験範囲。範囲外は開始位置へ戻り、試験全体を初期化する。
- 静止、横方向、上下方向、既存の浮遊射撃AI、遮蔽比較の5体。HP／燃焼／Windの衝突・AI中断復帰は既存コードを共用。
- `BP_PlaygroundTarget`は`WEVORAEnemy`を継承するC++クラス。死亡後3秒で復活。Rで全体リセット。プレイヤー死亡後2秒で再試行。
- リセットはHP、マナ、チャージ／CT、保持入力、飛行速度、対象の座標・移動位相・燃焼・外力・AI予告／攻撃待ち時間を戻し、弾・破片を除去する。
- 横／縦の移動は増分のsweep。Wind中は経路を一時停止し、その後は押された位置を基準に再開する。自動復活またはRで元の位置へ戻る。

## 保存済み設定の調査

元プレイヤーの巡航1000cm/s、歩行500cm/s、通常攻撃CT0.5秒、チャージ2秒、最大Power2倍。
弾はFire2200／Wind2600／Lightning3200cm/s、半径16cm、寿命4秒。4ShapeすべてProjectile。
Fireは直撃25＋燃焼5/s×3秒、Windは押出900＋Lift220、Lightningは単発25。
保存済みBlueprintの`ResolveSpellLaunch`等4関数のoverrideを維持する。
読み取りスクリプト: `Scripts/inspect_playground_baseline.py`。実測結果: `Saved/CombatPlayground/BlueprintDefaultsBefore.json`。

## アセット作成

`Scripts/create_combat_playground.py`をUE Python commandletで実行する。新規アセットが既に存在する場合は停止し、上書きしない。
既存敵BlueprintのReaction Rules／敵Projectileを新規ターゲットに転記する。
今後の編集は専用Blueprint／マップのDetailsで行う。再生成スクリプトによる既存調整の消去を避ける。

## 検証記録

着手前の`WEVORA.`: 23/23成功（`Saved/Automation/PlaygroundBefore/index.json`）。
各Stageと最終ビルドの結果はPROGRESSと最終検証記録へ追記する。NullRHI検証と実描画／実入力の評価を区別する。
