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

## Stage B: 偏差射撃と円形照準

- 円は目安。中心点から実際のsweep弾を発射する。通常52／RMB28 Slate単位、DPIに追随。外周はCharge／CTを表示する。
- `ResolveCastLaunch`で保存済みBPの解決後にCharge／Playground倍率を適用する。プレビューと実発射は同じスナップショットを使う。
- Playgroundの初期半径倍率1.75、最大Chargeでさらに1.65。Fire/Windの弾速は保存値を維持。距離上限9000cmと既存寿命4秒の早い方で消える。
- Lightningは12000cm/s。F3で発射時だけ1.5度／6000cm以内の補助を比較する。初期OFF。発射後は曲がらず、対象の移動先も予測しない。カメラと発射元双方の遮蔽を確認し、壁越し補助を拒否する。
- F2で実弾道、衝突半径、対象速度を表示。速度継承は専用PlayerのSpellCastComponentでOpt-in、既定OFF。
- 赤表示は射線遮断予告。発射元に障害物があれば発射禁止。途中の壁には弾が衝突する（環境への着弾検証も維持する）。
- 調整箇所: マップ内`Combat Playground - tuning and reset`のShooting／Tuning。発射位置・寿命・速度継承は専用PlayerのSpellCastComponent。

## Stage C: 少数のDeliveryから形を組み立てる

専用PlayerのSpellCastComponent → Shape ProfilesとDelivery Classesで変更する。旧Playerは4ShapeすべてProjectileのまま。

| 入力 | 配送・役割 | 初期仮説 |
|---|---|---|
| LMB短押し／保持解放 | 直線弾、ChargeでPower／半径増加 | Fire2640、Wind3120、Lightning12000cm/s、CT0.5秒 |
| E＋上へ直線 Thrust | 同じ直線Delivery、描いた入力への報酬 | 速度1.4倍、Power1.25倍、手動偏差撃ち |
| E＋左右 Sweep | 一回の横長volume | 前方1000×左右650×上下200cm、Power0.9、複数対象 |
| E＋円 Circle | 指定方向の短時間Field | 最大1800cm先、半径380、2.4秒、0.6秒毎、Power0.35 |
| E＋下 Slam | 足元より前下方へのArea | 半径420cm、Power1.5、近距離のリターン |

Sweep／Field／Areaは同じ`WEVORASpellVolume`。同一pulse内の重複を排除し、各対象へのLOSを確認する。Fieldはtimer駆動で寿命最大5秒、最小間隔0.1秒。
Field／Slamの中心も手元からのtraceで壁手前へ止める。範囲が壁をまたいでも、壁の向こうの対象は受けない。
属性効果は既存Reaction Rules。Fireの直撃／燃焼、Windの外力とAI復帰、Lightning単発を維持する。感電・連鎖は追加しない。
調整スクリプト`Scripts/configure_playground_spells.py`は専用Playerだけを設定し、既にDelivery Classesが設定済みなら停止する。

## 検証記録

着手前の`WEVORA.`: 23/23成功（`Saved/Automation/PlaygroundBefore/index.json`）。
各Stageと最終ビルドの結果はPROGRESSと最終検証記録へ追記する。NullRHI検証と実描画／実入力の評価を区別する。
