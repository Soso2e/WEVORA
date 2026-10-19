# Solo Arena Prototype — Issue #10

ソロ設計の正本は [SoloArenaDesign.md](SoloArenaDesign.md)。実装順序・接続点は [SoloArenaVerticalSliceImplementation.md](SoloArenaVerticalSliceImplementation.md)。
`feature/solo-arena-vertical-slice` は PR #9 を基にする作業ブランチ。PR #8 / #9 は依存するDraftで、マージしていない。
main のソロ文書3コミットを差分確認してcherry-pickし、ROADMAPの既存進捗と新方針を両方残した。

## 開始・操作

- Editorで `/Game/WEVORA/Maps/L_SoloArenaPrototype` を開いてPIE。
- 配布Playerは `WEVORA.exe /Game/WEVORA/Maps/L_SoloArenaPrototype -windowed`。
- 既定マップは引き続きCombat Playground。ソロは専用Map / GameMode / Directorによる明示的な起動。
- WASD / Space / Ctrl / Shift / Altの飛行操作は既存と同じ。LMB通常攻撃・長押しチャージ、Q属性切替、E長押し＋マウス移動で編み、RMB精密エイム、Y視点復帰。
- ウェーブ間のカードは1/2/3またはマウスで選択。選択中と結果表示中は攻撃・飛行を停止し、未完了入力を破棄。
- Rでランを放棄して再挑戦。結果画面のRetryボタンまたはEnterでも再挑戦。
- 保存失敗時は結果画面に明示し、Save againボタンまたは2で再保存。失敗した報酬を永続解放済みとは表示しない。

## ランと調整データ

`/Game/WEVORA/Solo/DA_SoloArenaPrototype` の `Waves / SpawnOffsets / PreparationSeconds / WaveTimeout / BoundsExtent / MinimumHeight / Upgrades / Trial` を調整する。
配布物・検証場の既存Blueprintやクラス既定値を強化適用時に書き換えない。

| 暫定値 | 内容 |
|---|---|
| Waves | 4 / 6 / 8 / 10体 → ボス1体、合計5ウェーブ |
| AliveCap | 3 / 3 / 4 / 4 / 1。コード側の安全上限16 |
| SpawnInterval | 1.5秒。1tickで最大1体出現 |
| HP | 60 / 75 / 90 / 105 / 650 |
| Preparation / timeout | ウェーブ前3秒、戦闘300秒で敗北 |
| BossWindup / BossRecovery | 1.4秒の固定照準予兆 / 2.2秒の移動・再攻撃停止 |
| Element | 弾・範囲の幅/高さ+20%、反応ルールの解禁条件は変更しない |
| Mobility | 飛行・地上速度+15%、地上マナ回復+25% |
| Survival | 最大HP+30、HP40回復 |
| カード上限 | 各4stack、半径合計+200%、速度+100%、回復+200%、最大HP+200の安全上限 |
| Trial | `LightningTrial` → 実装済みLightning。累計クリア数による解放ではない |

値と提供方法は試作仮説。現在は各系統の先頭にある取得可能な候補を選ぶ決定的な3択で、ランダム抽選は行わない。
候補が尽きるほどウェーブ数を増やす場合は候補・stack上限も調整する。候補が全て尽きると設定上の理由を表示して終了する。
タイムアウトや生成不能・死亡以外の敵削除は成功扱いにせず、理由つきの敗北とする。

## 戦闘・終了・保存

`Preparing → WaveActive → UpgradeChoice → Preparing → … → BossFight → Victory / Defeat`。
敵はRun Director自身が生成したものだけを数え、実際のHealthが0になってから処分する。
最終撃破後に1tick待って進行を確定し、同frameのプレイヤー死亡を敗北として優先する。
残HPのある敵を外部削除しても撃破報酬を得られない。プレイヤーの場外・死亡・ウェーブtimeoutで一度だけ終了する。

新しいArena Enemyは既存Enemyの追跡・攻撃・Projectile・Health・Reactionを再利用し、既存の8Zone ProfileとFire/Wind Ruleを持つ。
ボスは赤い固定照準線と着弾点を表示し、プレイヤーが予兆中に移動して回避できる。発射後は停止し、HUDに反撃可能な硬直時間を表示する。
別のダメージ計算や無敵・カード限定反応は追加していない。部位が燃焼すると橙色に変わる。

一時強化は専用コンポーネントでラン開始時の値を保持し、終了・放棄・再挑戦・EndPlayで解除する。
Directorはtimerを使わない。終了時には所有する敵・プレイヤーのDelivery・敵のProjectileを破棄する。
敵Projectileは発射元死亡後も残る安定したCombatScopeを持ち、次ランへの持ち越しを防ぐ。
飛翔弾は既存の寿命に加え、プレイヤー48 Delivery、敵1体8 Projectile（ハード上限16）で抑える。

SaveGameスロットは `WEVORA_Solo_Progress_v1`、schema Version 1。保存対象は試練IDと永続属性だけ。
初期属性Fire/Wind＋保存済みの実装済み属性が**次ラン開始時**に利用可能になる。現在のサンプル報酬はLightningのみ。
同じ試練は何度でも再挑戦でき、解放を重複付与しない。異なるschema・読めない保存・未対応属性は上書きしない。
水属性の戦闘実装・架空の水属性UIは追加していない。

## 検証

`Scripts/validate_solo_arena.ps1 -Phase A/B/C/D` で各段階を検証。Dは既存45件との名前照合、別Editorプロセス間の保存・再読込、全Blueprint、Editor/Gameビルド、3マップPackage、2マップのPlayer描画smokeまで実施する。
テスト用保存には専用GUIDスロットを使い、通常のプレイヤー保存を変更しない。
共有結果・最終件数・実行環境・警告の範囲はPROGRESSと自動テスト結果に記録する。

2026-10-19検証: A47 / B49 / C55 / D58件成功、既存45件を全て保持。最終テストの警告・エラー・未実行0。
別Editorプロセスでの保存・再読込各1件も成功。全349 Blueprint（/Game55件）はエラー・警告0。
UE5.8.3 Win64 Editor / Game、3マップのCook・Package・Archive、Solo / Playgroundの配布Player描画smoke（各452frame / 終了0）成功。
既存Content/Config772件のSHA-256不変。新規2資産込み774件を保持。
[58件の結果](SoloArenaAutomationResults.json)、[ビルド・Package・ハッシュ・警告](SoloArenaValidation.json) を参照。
EngineヘッダC4996、推奨MSVCとの差、既存のNVIDIA TSR警告は残る。人間の受け入れ確認は下記のとおり未実施。

## 人間の受け入れ確認（未実施）

1. ソロ用の新規保存で、Fire/Windのみが選べ、通常攻撃・チャージ・4形状・飛行を操作できる。
2. 連続増援・同時上限・敵弾と被ダメージ・ウェーブ全滅を確認する。
3. 2回以上のカード選択をマウスとキーボードで試し、選択中の誤射や選択後の入力残留がないことを確認する。
4. 強化なしでも右手Fire→腕Wind→胴体への局所伝播が発生する。
5. ボスの赤い照準線を避け、硬直中に反撃して実HPを削り、クリアする。
6. 結果でLightningの保存を確認し、再挑戦で一時強化が消え、Lightningを使用できる。
7. Playerを終了・再起動し、Lightningと試練クリアが維持される。敗北・場外・Rによる再挑戦も確認する。
8. Combat Playgroundへ戻り、5+1対象とF1/F2/F3/R、局所Fire/Windを確認する。

自動テスト・描画smokeは実入力・遊びやすさ・楽しさの確認ではない。PIEと配布Playerの人間による通しプレイ、狙いやすさ、予兆の読みやすさ、速度・難度・ラン時間のバランスは未確認。
専用ゲームパッド用のカード操作・正式モデル/リグ/Niagara/音・水や追加ボス・複数試練の選択メニュー・オンライン対応は追加していない。
