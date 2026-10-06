# Blueprintでの移動・魔法の調整

2026-10-06: 第一段階。`/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`に、
よく変更するデザイン判断を4つの関数グラフとして移行。
これらのノード変更はBlueprintのCompile / Saveだけで反映でき、C++ビルドは不要。
既存のEventGraph、Class Defaults、コンポーネント設定は維持。

## 挙動を変更する場所

BlueprintのMy Blueprint → Functionsで、次の関数を開く。
空の新規関数を作る必要はない。

| 関数 | 編集できる内容 | 呼び出すタイミング |
|---|---|---|
| `CalculateCameraFOV` | 速度からFOVを求める式、参照速度、補間、状態に応じたカメラ変化 | Characterのカメラ更新時 |
| `GetSpellMovementTarget` | Idle / Weaving / Shaping / ReadyToCastごとの移動倍率、状態条件の追加 | 移動物理更新の直前 |
| `ResolveSpellLaunch` | 属性・Gesture→Shape / Delivery、Power、弾の速度・半径・寿命・色の組み立て | 魔法編みからCast要求を受けた時 |
| `GetSpellRecoilSpeed` | Gestureや属性に応じた反動量・条件 | 発射に成功した時 |

初期グラフは以前のC++の挙動を再現する。C++には未移行の子クラス向けに従来の
フォールバックが残るが、このプレイヤーは保存済みBlueprintのオーバーライドを使用する。
Blueprintに親関数を追加すると、親の既定処理を利用する形にも戻せる。

例: Circleの反動を0にしたい場合は`GetSpellRecoilSpeed`のCircleのReturnを0にする。
編み中も通常速度にしたい場合は`GetSpellMovementTarget`のWeavingのReturnを1にする。
魔法速度にPowerを乗算したい場合は`ResolveSpellLaunch`のSpeedの計算へPowerを追加する。
これらの関数は同期的な判断専用。DelayやTimelineは使用せず、演出は既存の通知へ接続する。

`ResolveSpellLaunch`はOutLaunchとReturn Valueを返す。Return Value=falseで発射を拒否する。
新しいDeliveryを指定する際は`SpellCastComponent.DeliveryClasses`へ実装クラスを登録する。
AreaなどのEnumを選ぶだけでは配送実装は作成されない。
Aim方向と発射前の水平速度継承は、その後C++が実際のカメラ・プレイヤーから確定する。
Power / Speed / Radius / Lifetimeが無効な発射データはC++が拒否する。

## 数値・素材だけを変更する場所

| 調整対象 | Blueprintの場所 |
|---|---|
| ジャンプ、空中加速、重力、上昇、Shift、Alt、Recovery | Class Defaults → `WEVORA Movement` |
| カメラ基準FOV、速度による増加、補間速度 | Class Defaults → `WEVORA Movement / Camera` |
| カメラ距離、位置、位置・回転Lag | `CameraBoom`コンポーネント |
| 最大マナ・地上回復 | `ManaComponent` |
| 属性別速度・半径・寿命・色 | `SpellCastComponent / Element Parameters` |
| Gesture別の形・Delivery・速度/半径倍率 | `SpellCastComponent / Shape Profiles` |
| 魔法のPower、水平速度継承率、反動量 | `SpellCastComponent` |
| 体の属性切替演出、右手の継続演出、色、取り付け | `SpellSelectionEffectComponent` |
| Fireダメージ/燃焼、Wind押し出し/持ち上げ | 敵等の`SpellReactionComponent / Reaction Rules` |
| 魔法UI・選択時/編み中の追加演出 | `SpellWeavingComponent.OnSpellPresentationChanged` |
| 発射要求に伴う追加演出 | `SpellWeavingComponent.OnSpellCast`（発射成功の通知ではない） |

Class Defaultsやコンポーネントへの調整値の上書きは、C++の初期値より優先される。
魔法のPowerは対象側ReactionのMagnitudeに作用する。Fireのダメージ値自体は
`Element Parameters`ではなく対象側Reaction Rulesで調整する。

## C++に残す基盤と次の段階

物理直前の更新順序、衝突・着地・壁遮断、入力解除、マナ残量の更新、
認識の蓄積、発射Actorの生成、燃焼の時間管理は引き続きC++。
Space / Ctrl / Shift / Altの優先順位とホバー/上昇の切替そのものもC++に残る。
入力条件を頻繁に変更する段階では、次に飛行の状態選択を独立したBlueprint関数へ移す。
今回、全ての飛行物理や敵AIをBlueprintへ書き換えたわけではない。

## 検証・バックアップ

`WEVORA.Design.BlueprintRules`は保存済みBlueprintのオーバーライド所有、
30/60/120fpsでのFOV計算、4状態の移動倍率、Fire/Wind×4Gestureの発射データ・反動、
設定変更の反映、未設定Shapeの拒否を確認する。既存の`WEVORA.`テストも合わせて実行する。
実入力・描画・操作感はPIEで確認する。

一度限りの移行スクリプト: `Scripts/migrate_blueprint_design.py`。
既存グラフがある場合は実行を拒否するため、今後のノード編集を上書きしない。
変更前の保存済みアセットは`Saved/BlueprintDesignBackup/<日時>/`へコピー済み。
グラフの読み取り結果は`Saved/BlueprintDesignMigration.json`、
テスト結果は`Saved/Automation/BlueprintDesign/index.json`。

今回の検証結果: UE 5.8 Windows Editorビルド成功、Blueprintの警告をエラー扱いにした
コンパイル成功。全18テスト中16件成功、新規`BlueprintRules`と全魔法テストは成功。
飛行の`BurstAndBrake` / `HeldHoverAndNaturalFall`は失敗。
変更前アセットでの比較でも同じ2件が失敗（移動7件中5件成功）したため、
この移行で新たに発生した失敗ではない。テスト期待値と既存の調整値・飛行仕様の
切り分けは別途必要。移行後アセットへ復元したことをハッシュで確認。
起動ログには既存のGameFeatures / GameFeatureData設定不足によるエラーもある。
移行用Pythonの実行・グラフのコンパイル・保存自体は成功したが、
この起動時エラーで移行コマンドレットの終了コードは1。全ログがエラー0とは扱わない。
