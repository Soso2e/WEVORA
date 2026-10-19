# WEVORA Progress

最終更新: 2026-10-19

現在フェーズ: Solo Arena Vertical Slice（Issue #10、`feature/solo-arena-vertical-slice`、Draft PR #9の上に積む）

## 2026-10-19 Solo Arena Vertical Slice — Issue #10

設計正本は `Docs/SoloArenaDesign.md`。mainのソロ文書3コミットをcherry-pickし、ROADMAPの競合は新優先方針と既存の検証記録を両立して解消。
PR #8 / #9 / mainは変更・マージしていない。実装手順・操作・調整値・人間の受け入れ確認は `Docs/SoloArena.md`。

- Phase A (`4f6e624`): 専用GameMode / HUD / Run Director、段階的出現・同時上限、実HPによる撃破、敗北優先の状態遷移、timeout・場外・放棄・再挑戦。Editorビルド、**47/47成功**（既存45件保持）。
- Phase B (`2e633e5`): Element / Mobility / SurvivalのカードUIと1/2/3・マウス選択、半径・飛行速度・地上マナ回復・HPのラン内強化、stack上限、終了・再試行時の解除。選択中は未完了入力を破棄し、操作を安全に再開。ソロ初期Fire/Windと発射時の属性アクセス検査。Editorビルド、**49/49成功**。
- Phase C (`8a380a2`): 専用 `L_SoloArenaPrototype` と `DA_SoloArenaPrototype`、既存AI/Health/Projectile/Reactionを共用する8Zone敵・ボス。固定照準の赤い予兆と硬直、勝敗UI、試練IDごとの属性解放、version 1 SaveGame、保存失敗の表示・再保存・未知schemaの上書き防止。Editorビルド、**55/55成功**。
- Phase D (`3f475e1`): 保存マップ・実カードボタン・実Projectileの右手Fire→腕Wind→胴体・既存局所反応、ラン終了後の所有弾/状態/保留反応破棄、無効な調整値とtimeoutを追加検証。試練解放後の実Lightning発射も確認。**58/58成功**。
- 仮説は5ウェーブ・4回の3択・ボス1体。各属性の基本反応は強化取得で制限しない。Waterは実装・使用可能表示ともに追加していない。Lightningは試練初回クリア後、次のランから使用可能。
- ソロは明示的に専用マップを起動する。既定起動マップはCombat Playgroundのまま。既存の5+1対象、F1/F2/F3/Rと既存45件の回帰を維持。

| 検証 | 結果・証跡 |
|---|---|
| 環境 | Unreal Engine **5.8.3** (CL 58210709)、Win64 Development、MSVC 14.51.36260 |
| 実装前baseline | 45/45成功。`Saved/Automation/SoloBaseline/index.json` |
| 各段階Editor / 全WEVORAテスト | A47 / B49 / C55 / D58成功。`Saved/Automation/SoloA`〜`SoloD/index.json` |
| 最終自動テスト | **58/58、未実行0、テスト警告0・エラー0**。既存45件をテスト名で照合。`Docs/SoloArenaAutomationResults.json` |
| 別プロセス保存 | 専用GUIDスロットへ保存→Editor終了→別Editor起動→Lightningと試練IDを再読込成功。Write / Read各1件成功、通常のプレイヤー保存は変更しない |
| Blueprint | **349件、/Game 55件、エラー0・警告0・ロード失敗0**。保存なし。`Saved/Logs/SoloDBlueprints.log` |
| Win64 Game / 3マップPackage | Gameビルド、Solo / Playground / ThirdPersonのCook・Stage・Pak・Package・Archive成功、UAT終了0。`Saved/Logs/SoloDPlayerBuild.log` / `SoloDPackage.log` |
| Player描画smoke | Solo / Playgroundを別々にD3D12 / SM6で起動し、各452フレームで正常終了0。実入力なし。配布exeのSHA-256が最終Binaries版と一致 |
| 既存データ保護 | 最終Cook後も実装開始時Content/Config **772ファイル全てのSHA-256不変**。新規Map / DataAssetの2件もコミットのLFS SHA-256と一致。合計774件 |

ビルド・配布・新規資産のSHA-256と警告の機械可読記録: `Docs/SoloArenaValidation.json`。
配布exe SHA-256: `1409e03c176b7a4893f3a7537cec423e3c260bdfce508aede6560048b5adf10c`。
ローカル配布先: `Saved/SoloArenaPlayer/Windows/WEVORA.exe`（引数に `/Game/WEVORA/Maps/L_SoloArenaPrototype` を指定）。
コンパイルはEngineヘッダのC4996警告行がEditor96 / Game49、推奨MSVCとの差の警告あり。テスト・Blueprint警告は0。
Playerには既存のNVIDIA TSR 16-bit VALU無効化警告が各1件。Git LFS fsck、diff --check、PowerShell構文検査成功。

テストの成功は**人間のPIE/配布Playerの実操作・通しプレイや面白さの確認ではない**。
狙いやすさ、飛行と編みの操作感、カードの読みやすさ、ボス予兆・反撃・難度・ラン時間・再起動後の手操作確認は未実施。
ゲームパッド専用カードUI、複数試練の選択メニュー、水など追加属性、正式モデル/mGear/Niagara/音、オンラインは未実装。
再現: `Scripts/validate_solo_arena.ps1`（`-Phase A/B/C/D`、`-SkipPlayer`対応）。通常のプレイヤーSaveGameを自動テストで初期化しない。


## 2026-10-17 Combat Playground v1

Draft PR: https://github.com/Soso2e/WEVORA/pull/8 （base: main、head: feature/combat-playground）。
Stage A〜Eをコミット・Push済み。mainは`3ac0ace`のまま、マージしていない。

- Stage A: 独立`L_CombatPlayground`、専用Player/GameMode/Target Blueprint、5体の静止・横・縦・AI・遮蔽対象を作成・保存・再読込確認。既存マップ／Blueprintは変更しない。
- 敵のHP／Reaction／AIを継承し、自動復活、R全体リセット、死亡・範囲外からの再試行を追加。
- 実値調査: 保存済み巡航1000、Fire/Wind/Lightning弾速2200/2600/3200、半径16、CT0.5、Charge2秒。旧Blueprintのグラフと値を維持。
- Stage A Editorビルド成功。`WEVORA.` **26/26成功**（既存23＋新規ResetAndRespawn/MotionAndWind/FullTrialReset）。`Saved/Automation/PlaygroundStageA/index.json`。
- マップ生成終了コード0、保存済み5対象・GameMode確認（`Saved/CombatPlayground/MapCreation.json`）。実描画・入力・Playerビルドは後続Stageで検証する。
- 設計・再現手順: `Docs/CombatPlayground.md`。
- Stage B: 円形レティクル、共有プレビュー／発射解決、Charge半径、距離上限、弾道表示、Lightning12000cm/s＋F3切替の限定補助（既定OFF）。Editorビルド成功、`WEVORA.` **28/28成功**。弾距離／Charge／無効値拒否、補助OFF・他属性・角度外・遮蔽を追加検証（`Saved/Automation/PlaygroundStageB/index.json`）。
- Stage C: 専用BPのShape ProfilesでThrust（編みに速度1.4／Power1.25）、Sweep（横長一回）、Circle（短時間pulse Field）、Slam（近距離Area）を設定。同一範囲Deliveryを共用し各対象へのLOSで壁越し作用を拒否。Editorビルド成功、`WEVORA.` **29/29成功**（`Saved/Automation/PlaygroundStageC/index.json`）。旧23件も成功。新規は複数対象Sweep／範囲外／遮蔽／Field持続・寿命とtimer終了を実Worldで検証。
- Stage D: Charge核と光量・サイズ、安定した弾核と速度連動Trail、短い属性色Burst、Volume輪郭、対象フラッシュ、既存Widget統合HUD／ガイド／状態・ヒット表示を追加。処理後のLaunch／Impact通知を公開。上限Delivery48/Player・Burst32/World、視覚部品は衝突なし。Editorビルド成功、`WEVORA.` **30/30成功**（`Saved/Automation/PlaygroundStageD/index.json`）。追加テストでCharge拡大／Idle停止／衝突無効／実HP差通知／Flash寿命／統計resetを確認。

### Stage E 最終検証・成果物

| 項目 | 実施結果と証跡 |
|---|---|
| UE 5.8.3 Win64 Development Editor | 成功。`Saved/Logs/PlaygroundFinalEditorBuild3.log` |
| 最終`WEVORA.` | **33/33成功、未実行0、テスト警告0・エラー0**。既存23＋新規10。`Saved/Automation/PlaygroundFinal/index.json`、共有要約`Docs/CombatPlaygroundAutomationResults.json` |
| 新規の最終検証 | 保存済みマップと5対象／専用GameMode、3属性×4形の保存済み配送、実CastのProjectile／Volume生成、偏差なしの非命中と偏差ありの実sweep命中 |
| Blueprint全体 | **349件成功（/Game内55件）、エラー0・警告0**。終了コード0。`Saved/Logs/PlaygroundBlueprintCompile.log`。保存なし |
| Win64 Development Playerコード | 成功。`Saved/Logs/PlaygroundFinalPlayerBuild.log`。アーカイブ内exeのSHA-256が最終Binaries版と一致 |
| Cook / Stage / Pak / Package / Archive | Playground＋旧ThirdPersonの2マップで成功、UAT終了コード0。`Saved/Logs/PlaygroundPackaging.log`。出力`Saved/CombatPlaygroundPlayer/Windows/WEVORA.exe` |
| 単体起動 | D3D12/SM6でPlaygroundロード、452フレーム、通常終了コード0。`Saved/Logs/PlaygroundPlayerRun.log`。15秒benchmark、実入力なし |
| 実描画の限定確認 | Editor -gameのウィンドウとPlayerのゲーム内Screenshotでフィールド／5対象／円／HUD／ガイドを目視。床の強い模様を専用matte材質へ修正して再Cook。最終Player初期フレーム`Docs/Images/CombatPlayground.png`。Magic VFXの動的な見え方、飛行入力、PIEは未確認 |
| 既存データ保護 | 既存Content/Config763ファイルのSHA-256比較で既存Contentは一致。意図的変更はConfig2件のGameDefaultMapとMapsToCookのみ。既存マップ／Blueprint調整値／描画設定は維持。`git lfs fsck`成功、`git diff --check`成功 |

初期のコンパイルで不足include／テストAPI型のエラーを修正した後、上記の最終ビルド・全テストを実行。
MSVC 14.51の推奨版差とエンジンAPIの非推奨警告は残る。Playerには既存ドライバーのTSR 16-bit無効化警告・初回PSO作成の記録あり。全ログの警告0・性能確認済みとはしない。
Editor -gameでは既存ToolsetRegistry Python初期化の`PythonTestRunner`未定義エラーも記録されたが、ゲームWorldは開始。CompileAllBlueprints／Cook／Playerの最終検証ログに同エラーはない。

**未完了の受入確認**: PIE／Playerでの実キーボード・マウス、飛行中の照準・4Gesture・Chargeの操作感、VFXの動的な見え方、30/60/120fpsの体感、面白さとバランス判断。
Windows入力確認中、UnrealPakのWindowsセキュリティダイアログが前面にあり停止。Computer Useの規則（`SKILL.md`が必須参照する`docs/guidance.md`の「Do not act on security/privacy permission requests.」）により自動操作せず、ユーザーに閉じる操作を依頼した。
技術検証の成功と人間のゲーム評価を分け、Draftを維持する。mainへはマージしない。

具体的なPIE手順12項目、全調整場所、主要判断、残課題は`Docs/CombatPlayground.md`。
再実行: `Scripts/validate_combat_playground.ps1`。変更ファイル一覧は`Docs/CombatPlaygroundFiles.md`。

## 2026-10-17 PR #7: work → main 安定化

対象PR: https://github.com/Soso2e/WEVORA/pull/7 （既存Draftを継続）。
調査開始時は`main=1b5844c`、`work=e758fb8`、17 commits ahead / 0 behind、43ファイル差分。
ローカルworkをfast-forwardして調査。mainへの直接コミット、マージ、force push、ブランチ削除は行わない。

### 調査・修正

- 既知の移動失敗は、保存済みBlueprint調整値との期待値不一致とテストの着地によるもの。
  Blueprintの巡航1000 / ダッシュ加算1200 / WASD降下倍率0.9に対し、旧テストはC++初期値1200 / 1800 / 0.8を前提としていた。
- 実測: Burst前の水平速度1000、低速からのShift直後1502.5。自然落下-588、WASD落下-529.2（比率0.9、30/60/120 fps共通）。
  BurstAndBrakeの旧条件はAlt検証中に床へ着地し、Z=92.15 / Walkingとなっていた。
- 移動テストを保存済み設定の加算量・上限・減衰・重力倍率で検証する形へ修正。
  BurstAndBrakeの開始高度を10000へ上げ、落下を受け止める時間3秒と空中前提のassertを追加。
  失敗assertを削除して成功扱いにはしていない。ゲームプレイの実装・調整値は変更不要と判断。
- `LightningHit`を追加。保存済みプレイヤー/敵/発射Blueprintを使い、QでのLightning選択、通常/最大チャージの実sweep命中、HP100→75/50、非燃焼・非変位・追加持続ダメージなしを確認。
- 移動READMEの空中ジャンプ無料、弾の速度継承が常時有効という古い記述を現行実装へ訂正。
  詳細な機能範囲・再現コマンド・PIE手順は`Docs/BaselineValidation.md`。

### 今回実行した検証

| 項目 | 結果 / 証跡（Savedはローカルのみ） |
|---|---|
| UE 5.8.3 Win64 Development Editor | 成功。`Saved/Logs/BaselineFinalEditorBuild.log` |
| 修正前 `WEVORA.` 全テスト | 22件中20成功、既知2件失敗を再現。`Saved/Automation/BaselineBefore/index.json` |
| 修正後 `WEVORA.` 全テスト | **23/23成功、未実行0、テスト警告0・エラー0**。移動7、魔法14、敵AI1、Blueprintルール1。`Saved/Automation/BaselineFinal/index.json` / `Saved/Logs/BaselineFinal.log`。共有用要約`Docs/BaselineAutomationResults.json` |
| Blueprintコンパイル | 346件成功（/Game内52件を含む）、コンパイルエラー0・警告0・ロード失敗0、終了コード0。保存なし。`Saved/Logs/BaselineBlueprintCompile.log` |
| Win64 Development Gameコードビルド | 成功。最終テスト修正も含めた再ビルド`Saved/Logs/BaselineFinalPlayerBuild.log` |
| 既存マップのCook / Stage / Pak / Package / Archive | 成功、UAT終了コード0。`Saved/Logs/BaselinePackaging.log`。最終Player再ビルド後もCook済みデータを使って再Stage/Pak/Package/Archive成功（`BaselineFinalPackaging.log`）。出力`Saved/BaselinePlayer/Windows/WEVORA.exe` |
| パッケージ版単体起動 | D3D12 / SM6で`Lvl_ThirdPerson`のロード・World開始・452フレームの実行と通常終了を確認、終了コード0。最終再ビルド/再Stage後の再起動も成功。`Saved/Logs/BaselinePlayerRun.log` / `BaselineFinalPlayerRun.log`。`-benchmark -seconds=15`、実入力なし。画面の目視/操作感の成功とは扱わない |
| アセット保護 | Content / Config追跡対象763ファイルの作業前後SHA-256一致、`.uasset` / `.umap` / Config変更なし。`git lfs fsck`成功、`git diff --check`成功 |

ビルドにはMSVC 14.51（UE推奨14.50より新しい）の警告とエンジンヘッダーの非推奨API警告あり。
途中の診断用テストのコンパイルエラーは修正済み。CookerがDLLを使用中のリンク失敗（LNK1104）はCook終了後の再リンクで解消。
Cookでは名前解決警告、Playerではドライバーに伴うTSR 16-bit無効化と初回PSO生成のヒッチ記録あり。
全ログの警告0や性能検証済みとは主張しない。

### 現在の機能範囲と残課題

- Fire / Wind、通常攻撃、チャージ、4Gesture認識、空中Cast、命中/燃焼/押出、壁遮断、照準共有、敵AI、Blueprintの互換性は上記Headless回帰テストで成功。
- Lightningは選択・共通Projectile・属性別の仮演出・単発ダメージまで。感電、連鎖、部位別作用は未実装。
  Shapeは4種あるが配送は共通の弾。Wind×Burningはルール拡張のテスト例で、標準の複合反応としては未設定。
- レティクルは既存の十字表示。円形化はCombat Playground段階。Lightningのジェスチャートレイル色はWindと共通。
  正式HUD、Niagara素材差替の実描画、ネットワーク同期、死亡画面/リスポーンは今回対象外。
- **未検証: PIE/Playerでの実キーボード・マウス入力、照準/操作感、日本語HP/燃焼UI、VFXの見た目、30/60/120 fpsの体感。**
  `Docs/BaselineValidation.md`末尾の5項目を、人間がPIEとパッケージ版で確認する。
- マージ判断: コード/自動テスト/アセット保護/Playerビルド・起動の技術的阻害要因は解消。
  ただし上記の実プレイ確認が残るため**Draft維持、現時点の無条件マージは推奨しない**。
  人間の確認結果をPR #7へ追記してからDraft解除・mainへの統合を判断する。

以下の過去記録にある失敗・未確認事項は当時の結果。上記の今回結果と区別する。

## 2026-10-08 Player攻撃の確認

### 的の頭上HPと燃焼表示

- Playerの表示Widgetに、既存WEVORAEnemyの頭上`HP 現在 / 最大`と燃焼中の`燃焼中`を追加。画面外・死亡・壁越しは非表示。数字は実HealthComponent、状態はSpellReactionComponentから取得。
- Enemyに仮炎の発光コーン6個と揺れるオレンジのライトを追加。既存Glow素材を利用、燃焼状態/生存状態に従って表示・停止。炎は当たり判定/影なし、専用サーバーでは非表示。既存Blueprintでも継承、追加素材/移行なし。
- UE 5.8 Win64 Development Editorビルド成功。関連魔法/敵/TPS照準テスト12/12成功・テスト警告0・失敗0。既存実命中テストへ炎Mesh/Materialの読込、燃焼での表示開始/終了、衝突無効の確認を追加。`Saved/Automation/EnemyStatus/index.json`、`Saved/Logs/EnemyStatusTests.log`。実画面の頭上UI/日本語/仮炎の見た目はPIE確認待ち。

### ジェスチャーの仮トレイル

- E保持中、認識と同じ反転済みLook入力をイベントで表示Widgetへ通知。画面中央から光る軌跡を描く仮トレイルを追加。Fireはオレンジ、Windは水色、白い芯＋淡い外周。尾は約0.45秒でフェードし、次の描き始めでリセット。表示用履歴は最大128点、購読は操作対象変更/Widget終了時に解除。
- Niagara素材追加なし。認識/魔法/照準の挙動は維持。設定と確認操作は`Source/WEVORA/Spell/README.md`。
- UE 5.8 Win64 Development Editorビルド成功。既存魔法・TPS照準のHeadlessテスト11/11成功、テスト警告0・失敗0。`Saved/Automation/GestureTrail/index.json`。差分チェック成功。トレイルの実画面表示/見た目はPIE確認待ち。

### 攻撃発動のユーザー確認

- ユーザーから「攻撃ができることは確認できた！」との報告を受領。Playerで攻撃を発動できることを実操作で確認済みとして記録。
- 案内した操作: 左クリック保持 → E保持中にマウスで形を入力 → Eを離す → 左クリックを離して発射。QでFire / Windを切替。
- 今回確認できた範囲は攻撃の発動。敵への命中・HP減少、Fireの燃焼、Windの押し出し、各Gesture、飛行中の攻撃、見た目・操作感の詳細は、この報告だけでは確認済みにしない。
- 実行環境（PIE / 単体起動 / パッケージ版）の明示はないため、Playerビルドの確認結果とは扱わない。今回の変更は確認記録のみ。

### TPS照準改善

- 写真と現行コードを照合。中央にキャラクターが重なるカメラ、レティクル未表示、移動速度の弾への加算、Gesture入力でカメラが動くことを改善対象とした。
- 肩越しカメラ（SocketOffset: 0/85/55cm）、中央レティクル、RMB保持で距離300cm/FOV70の精密照準、E保持中の視点固定、カメラ回転Lag解除を追加。既存Blueprintのカメラ設定読込後に適用。移動/属性/Gesture判定は維持。
- レティクルとCastで同じカメラ→対象点→Player発射位置の照準計算を使用。対象は緑、弾半径を含む射線遮断は赤い×。発射位置の壁遮断を維持。飛行速度の弾への加算は明示Opt-inへ変更し、旧Blueprint値の残存でも既定は無効。
- 肩越し・移動中の実命中とGesture中の視点維持、カメラだけ見通せる遮蔽物の予告/弾停止、至近壁でのSpawn禁止を自動テストへ追加。
- 初回: 新規UIと照準コードのC++コンパイル成功。開いているUnrealEditorがDLLを保持してリンク失敗（LNK1104）。Editor終了後の再ビルド・新規/既存テストは待機中。PIEの描画/操作感は未確認。
- ユーザーのEditor終了後、UE 5.8 Win64 Development Editorの再ビルド成功。Headless全20件中18件成功、2件失敗。新規TPSAim/TPSCoverPreviewと既存の魔法・敵・Blueprintルールは成功。失敗は以前の記録と同名の移動`BurstAndBrake` / `HeldHoverAndNaturalFall`（今回の原因比較は未実施）。レポート: `Saved/Automation/TPSAim/index.json`。
- カメラの左右配置、RMB保持でFOV70へ収束、解除後のFOV復帰もTPSAimに追加。最終再ビルド成功、照準2件の再実行は2/2成功・テスト警告0・失敗0。レポート: `Saved/Automation/TPSAimFinal/index.json`。差分チェック成功。NullRHIによる論理/衝突確認であり、実際のレティクル描画・PIE操作感・Playerビルドは未確認。

## 2026-10-06 移動調整

### Blueprint移行・第一段階（移動/カメラと魔法を優先）

- `BP_ThirdPersonCharacter`へ`CalculateCameraFOV` / `GetSpellMovementTarget` / `ResolveSpellLaunch` / `GetSpellRecoilSpeed`の4関数を追加・保存。速度連動FOV、編み状態別移動倍率、属性/形からの発射データ組み立て、Gesture別反動のルールはBlueprintで編集でき、以降この範囲の変更にC++ビルド不要。
- C++にBlueprintNativeEventの呼び出し境界と互換フォールバックを追加。物理直前更新、マナ管理、Aim/壁遮断/Spawnはネイティブに維持。無効なBlueprint発射パラメータはSpawn前に拒否。既存の空中ジャンプ消費/部分マナダッシュ、保存済みBlueprint設定とEventGraphを保全。
- UE 5.8 Windows Editorビルド成功。移行Blueprintの警告をエラー扱いにしたコンパイル成功。新規`WEVORA.Design.BlueprintRules`と魔法テストは成功。全18件中16件成功・2件失敗。
- 失敗は`WEVORA.Movement.BurstAndBrake` / `HeldHoverAndNaturalFall`。変更前の保存済みBlueprintでも移動7件中5件成功・同じ2件が失敗。比較後は移行アセットを復元しハッシュ一致を確認。既存期待値/調整値の切り分けは未解決、今回の移行回帰と断定しない。
- 起動時の既存GameFeatureData設定不足エラーで移行コマンドレット終了コード1。Python実行・グラフCompile/Save成功は別途確認済み。ログ全体エラー0とは記録しない。実入力・見た目・操作感はPIE未確認。
- 調整手順とC++に残す範囲: `Docs/BlueprintDesign.md`。一度限りの移行: `Scripts/migrate_blueprint_design.py`（既存の移行関数を上書きしない）。バックアップ: `Saved/BlueprintDesignBackup/20261006_173612/`。結果: `Saved/Automation/BlueprintDesign/index.json`、比較: `Saved/Automation/BlueprintDesignBaseline/index.json`。

- 変更前に`git pull --ff-only origin main`を実行し、最新main `1b5844c`と一致を確認。既存の隔離checkout `work`で作業。
- 空中操舵加速度を新しい`AirSteeringAcceleration`（初期値550 cm/s²、従来2100の約26%）へ分離。地上加速度、ダッシュ追加速度、最高速度、魔法状態の移動倍率は維持。
- Shift押下時は単発ダッシュ、保持中は水平慣性を残すマナホバー。Enhanced InputのCompleted/CanceledとキーReleasedで保持解除。既存`DoBurst()`は単発APIとして維持し、保持入力向け`DoBurstStart/End`を追加。
- Altは直接ブレーキ＋ホバー。Ctrl優先、Space上昇、マナ不足時の自然落下、UnPossessでの入力リセットを維持。
- 無入力時の頂点浮遊・入力猶予・飛行解除後の低重力を廃止。WASD保持中の滑らかな垂直減衰は維持。支え入力をすべて離すと次の移動更新で通常重力に戻る。旧浮遊UPROPERTYとCoasting列挙値はBlueprint互換性のため残す。
- 既存飛行テストを新仕様に更新し、30/60/120 fpsで自然落下、空中加速、Shift保持/解除/重複押下、Alt直接ホバー、Ctrl優先、マナ不足、操作解除を確認する回帰ケースを追加。
- 検証済み: 差分と入力経路の静的確認、`git diff --check`。UE_ROOTは設定されているが実体がなく、今回のEditorビルド・Automationテストは未実行。過去の成功記録を今回の検証結果として扱わない。実入力・操作感はPIE確認待ち。
- 調整値と確認手順: `Source/WEVORA/Movement/README.md`。

## 現在の状態

- 実装済み・未確認: 既存Weaving→Spell Data→Delivery→対象Reaction Ruleを接続。FireはDamage25＋Burning5HP/秒×3秒、WindはKnockback900cm/s＋Lift220cm/s。既存飛行/入力/Cancel/Enemy HPを維持。
- 実装済み・未確認: 初期マップ`/Game/ThirdPerson/Lvl_ThirdPerson`へ既存`BP_WEVORAEnemy`を1体配置・保存。ラベル`WEVORA_CombatEnemy`、座標`(1100, 0, 452)`、HP100・ダメージ時の仮HP表示を有効化。地形を維持し、Map/外部Actorを再読込して保存を確認。配置スクリプトは既存敵を上書きしない。
- 完了: UE 5.8 Mac Editorビルド成功。最終Headless自動テスト14/14成功・テスト警告0/エラー0。Fire Forward実命中と燃焼終了、Wind押し出し/Lift、壁遮断、AI再開、State/Shape RuleとPower、世界Actor/物理Body、既存飛行/敵/入力状態の回帰を確認。保存済み初期マップの敵ReceiverとPlayer BlueprintのShape設定継承も確認。
- 未確認: PIEの実キーボード/Viewport入力、動きながらの狙いやすさ、操作感、球体/属性ライトの視認性、難易度、Playerビルド。人間側の確認手順は`Source/WEVORA/Spell/README.md`。マップ保存の検証とHeadlessの論理検証は、描画/実プレイの証明ではない。
- 制限: 全Gestureは共通の球を1発発射（Thrust速度1.2倍・Circle半径1.5倍）。Gesture軌跡からPowerの自動算出/Modifier/魔法マナ消費/正式HUD/VFX/SE/死亡画面/リスポーン/同期は今回対象外。
- 実装済み・未確認: 無消費の勢いあるジャンプ、頂点付近の約1秒浮遊と自然落下、空中WASDのマナ消費ホバー、Space長押し上昇、Ctrl優先の即時降下、強化したShift、Alt減速＋静止ホバー、入力終了/キャンセル/操作解除を実装。旧地面Traceホバーを廃止し、Walking/Fallingの床・衝突・着地を使用。
- 実装済み・未確認: 飛行用ManaComponent、地上回復、変更通知、仮マナ/飛行状態表示を追加。空中回復なし、マナ不足では高度維持と加速を停止。マナと速度・時間・重力はBlueprintで調整可能。
- 完了: UE 5.8 Editorビルド成功。最終Headlessテスト9/9成功（飛行4件、既存魔法4件、敵AI1件）・テスト警告0・失敗0。保存済みキャラクターBlueprint、World tick、実sweep着地、浮遊時間、マナ消費/回復、入力猶予、Ctrl優先、上昇、Shift後の速度維持/上限、Alt減速を確認。
- 未確認: 実キーボード/Viewport入力、PIEでの飛行操作感、カメラ、アニメーション、天井/壁/坂での体験、30/60/120 fpsの体感、マナ/難易度調整、Playerビルド。ユーザーの指示に従い実際の検証はユーザーが行う。手順は`Source/WEVORA/Movement/README.md`。
- 制限: 飛行用マナの魔法Cast消費への接続、正式HUD、マルチプレイヤー同期は未実装。
- 完了: `BP_WEVORAEnemy` / `BP_WEVORAEnemyProjectile`を`Content/WEVORA/AI`へ作成・保存・コンパイル。浮遊追尾、距離/視線による検出、予備動作、照準射撃、クールダウン、弾の壁衝突・寿命を実装。
- 完了: 再利用可能なHealthComponentを敵とWEVORACharacterへ組込。標準UnrealダメージによるHP減少、被弾時の仮HP表示、HP0の標的除外、敵の消滅、Blueprintイベントを追加。
- 完了: UE 5.8 Editorビルド成功。Headless自動テスト4件（敵AIと既存魔法3件）成功。敵AIは保存したBlueprintの読込・実移動・検出・照準・射撃間隔・実sweep衝突によるHP減少・壁遮断・検出範囲・死亡・購読解除を確認。弾は視線/カメラのTraceを妨げない設定。
- 未確認: 敵BlueprintのPIEでの描画・追尾・回避・難易度、Playerビルド。開いているEditorは作業保存後に再起動して新クラスを読み込むこと。
- 制限: 敵は直進追尾で経路探索なし。シングルプレイヤー向け。死亡画面/リスポーン/ネットワーク同期は未実装。魔法から敵へのダメージは共通Projectileで接続済み。
- 完了: 魔法編みの既存状態・属性・Cast処理を調査。選択/状態通知と表示専用コンポーネントを追加。
- 完了: Characterへ自動組込。Fire/Windの色付き仮ライト、属性別Niagara設定、Idleの短時間プレビュー、編み中の継続表示、Cast/キャンセル/EndPlay時の停止を実装。
- 完了: UE 5.8 WEVORAEditor Win64 Developmentビルド成功。
- 完了: Headless（NullRHI）自動テスト3件（Gestures / StateFlow / SelectionEffects）成功。素材なしの属性色、Idleタイマー、編み中継続表示、Cast/キャンセル、欠落プロフィール、EndPlay購読解除を確認。git diff --check成功。
- 作業中: なし。最小戦闘ループ、飛行・敵の体験確認と魔法の視覚調整はユーザーのPIE確認待ち。
- 未確認: PIEでの入力と見た目、ライト位置・明るさ、実Niagara素材でのパラメータ反映、Playerビルド。
- 問題: 最初のビルドで既存DoRecenterViewのController変数が親クラスメンバーを隠すC4458が発生。ViewControllerへ改名後ビルド成功。
- 問題: 自動テストの初回・2回目はEndPlay購読解除の検証が失敗。テスト用WorldのActor初期化が不足していたため、初期化と終了のライフサイクルを明示して再実行し、3回目は全件成功。3回目のWorld context警告もテスト側の不要なActor Destroyを除去して解消し、最終4回目は3件成功・テスト警告0・失敗0。現在の自動確認に未解決の失敗なし。
- TODO: Fire/Wind用のNiagara素材を制作・設定し、PIEで確認。
- 完了: 属性切替の瞬間は体側の短時間演出、編み中は右手hand_rに追従する表示へ分離。編み中のQでも体側の演出と右手側の属性更新を同時に実行。BodyOffset/HandOffset/HandSocketNameと属性別SelectionSystemで個別調整可能。
- 完了: Editorをユーザーが保存・終了後、UE 5.8 Editorビルド成功。Headless魔法テスト4/4成功・テスト警告0。実BP_ThirdPersonCharacterのhand_r存在、取り付け先、手のWorld位置との一致、旧固定オフセットの置換、体側との独立表示を確認。
- 未確認: 右手アニメーション中の視覚的な追従、体側の高さ・明るさ、素材設定後のPIE表示。発射・着弾VFXは引き続き未実装。
- 次にやること: ユーザーがEditorを開き直し、`Lvl_ThirdPerson`でSpace→空中Altホバー→LMB＋E＋上マウス→E解放→画面中央へ敵を狙い直す→LMB解放。Fire命中と敵HP100→75→燃焼終了時60を確認。QでWindのKnockback/LiftとAI再開も確認。感触の判断後に必要な値だけ調整。

## 2026-10-05: 汎用魔法基盤

- GitHubのorigin/main最新（7bbf072）へfast-forward後、`fix/spell-reaction-foundation`で実装。
- 変更前: Fire/Windは同じ弾で25ダメージ。HitがHPへ直結し、敵AIが外力速度を止めていた。
- 実装: Spell DataのElement / Shape / Delivery / Power / Direction、設定によるGesture→Shape、
  抽象Delivery Actor、対象側Reaction ComponentとRule配列。既存入力・飛行・Projectile・HP・敵AIを再利用。
- 初期作用: FireはDamage25＋Burning5HP/秒×3秒、WindはKnockback900cm/s＋Lift220cm/s。
  Windの直接Damageを廃止。敵AIは外力移動中だけ休止し、その後再開。
- 拡張: RequiredState / 任意Shapeを条件にRuleを追加。Wind×Burningは設定だけで追加可能。
  今回の配送はProjectileのみ。Sweep/Circle/Slamも既存互換の仮Projectileで、専用配送は未実装。
- Mac Editorビルド成功。既存テストの即returnするfor文をClang警告エラーに合わせてiterator条件へ修正。
- 自動確認: 最終14/14成功、テスト警告0/エラー0。世界Actorと物理Bodyへの作用も確認。
  保存済み初期マップの敵Receiver / HP表示、PlayerのShapeProfiles=4を読み取り検証。
  結果:`Saved/Automation/SpellFoundationFinal/index.json`、ログ:`Saved/Logs/SpellFoundationFinal.log`。
  起動時にUE内部のUnifiedError自己確認ログは出るが、WEVORAのテスト結果に失敗なし。
- PIEの実入力・描画・プレイフィール、Player/Windowsビルド、ネットワーク同期は未確認。
- 詳細なEditor手順・クラス・調整値・処理フロー・次の実装は`Source/WEVORA/Spell/README.md`。
  上記の過去記録にある「Wind25ダメージ」は今回の実装で置換。

## 確認方法

- 最小戦闘ループ/既存マップ/実入力手順: `Source/WEVORA/Spell/README.md`。結果:`Saved/Automation/SpellCombat/index.json`、ログ:`Saved/Logs/SpellCombatAutomation.log`、ビルド:`Saved/Logs/SpellCombatBuild.log`、配置:`Saved/Logs/CombatMapPlacement.log`。
- 飛行操作/調整/PIEチェック: `Source/WEVORA/Movement/README.md`。最終自動テスト結果: `Saved/Automation/FlightFinal/index.json`、ログ: `Saved/Logs/FlightAutomationFinal.log`。公開入力APIによるHeadless確認であり、実入力・描画・操作感の証明ではない。
- 敵の配置/設定/確認手順: `Source/WEVORA/AI/README.md`。
- 敵を含む自動テスト: UnrealEditor-Cmd.exe <WEVORA.uproject> -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests WEVORA." -TestExit="Automation Test Queue Empty"。
- 敵テスト結果: `Saved/Automation/EnemyAI/index.json`、ログ: `Saved/Logs/EnemyAutomation.log`。
- ビルド: UE_5.8/Engine/Build/BatchFiles/Build.bat WEVORAEditor Win64 Development -Project=<WEVORA.uprojectの絶対パス> -WaitMutex -NoHotReloadFromIDE。
- 自動テスト: UnrealEditor-Cmd.exe <WEVORA.uproject> -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests WEVORA.Spell" -TestExit="Automation Test Queue Empty"。
- テスト結果: `Saved/Automation/SpellSelection/index.json`、ログ: `Saved/Logs/SpellSelectionAutomation.log`。
- PIE: IdleでQを押して属性色が短時間表示され消える。LMB中は表示が継続し、Qで切替。Eで形を作り直しても表示は継続。Cast・途中キャンセルで消える。
- 繰り返し操作、Possess解除、PIE終了で旧表示が残らないことを確認。
- Niagara設定後: READMEの4つのUser Parameterを素材側へ接続し、属性/形/状態の反映を確認。
- WASD / Space / Ctrl / Shift / Altの既存移動、視点リセットもPIEで確認。

## 設計メモ

- 最小戦闘のブランチ`fix/minimal-spell-combat`（直前の飛行実装から継続）。`FWEVORASpellContext`は入力空間の構成データとして維持。`FWEVORASpellLaunch`にWorld Directionと解決済み速度/Damage/Radius/Lifetime/Colorを保持。ElementParametersとResolveSpellが挙動決定、Projectileが実移動/Hit、既存HealthがDamageを担当。今後のEnergy/Modifierはこの境界を拡張し、完成魔法名クラスは増やさない。
- 照準はカメラ中央からWorldStatic/WorldDynamic/PawnへTrace、プレイヤー視点位置からその点へ発射。発射区間をSphere sweepして近い壁の通り抜けを防止。飛行時の体の向きとは独立。編む操作は既存仕様のままカメラも動くため、E解放後/LMB解放前に狙い直す。狙いやすさは人間側で判断。
- 飛行改善のブランチ作成・開発後コミットをユーザーが指示（2026-10-05）。ブランチ`fix/inertial-mana-flight`。Push/PRは実施しない。実際の検証はユーザーが担当。
- 飛行の力と入力は専用CharacterMovementComponentから物理処理前に更新する。標準MovementはActor Tickより先に動くため、Actor Tickで力を変更すると1フレーム遅れる。Actor Tickはカメラ/表示のみ。マルチプレイヤー予測・同期は今回対象外。
- 初期値: ジャンプ900 cm/s、浮遊1秒＋重力復帰0.35秒、入力猶予0.2秒、通常重力1.2、Ctrl重力2.4。マナ最大100・ホバー8/秒・上昇18/秒・Shift12/回・地上回復20/秒（消費後0.75秒待機）。Shift加算1800 cm/s・上限3200 cm/s・クールダウン0.45秒。体験確認後に調整する。
- Git運用: ユーザーより必要な区切りでのコミットを許可（2026-10-04）。関連変更だけを対象とする。Push/PRは別途指示に従う。
- 認識・Cast処理と表示を分離し、OnSpellPresentationChangedで状態とContextを通知。表示のための毎フレーム監視は行わない。
- ElementEffectsは属性Enumをキーとする設定。System未指定時は仮ライトのみ、プロフィール未指定時は旧表示を停止。
- Niagaraコンポーネントと仮ライトは使い回し、EndPlayで解放。専用サーバーには生成しない。
- 魔法のNiagara素材、正式な発射/着弾演出、ネットワーク同期は未実装。魔法Projectile/敵の射撃ダメージとも既存HealthComponentへ接続済み。
- プロジェクト内のAGENTS.md/Concept.md/既存PROGRESS.mdは見つからず。Spell/README.mdを参照。BRAINの20_Projects/30_Decisionsの対象語検索では関連ノートなし。

## 変更履歴

| ID | 内容 | 結果 | 変更ファイル | 次 |
|---|---|---|---|---|
| 2026-10-04-01 | 魔法選択エフェクト基盤、変更通知、Niagara依存、仮ライト、設定手順、ライフサイクルテストを追加。既存C4458を変数改名で修正。Editorビルド成功。 | 実装済み・未確認 | `Source/WEVORA/Spell/WEVORASpellSelectionEffectComponent.h/.cpp`, `Source/WEVORA/Spell/WEVORASpellWeavingComponent.h/.cpp`, `Source/WEVORA/Spell/README.md`, `Source/WEVORA/WEVORACharacter.h/.cpp`, `Source/WEVORA/WEVORA.Build.cs`, `Source/WEVORA/Tests/WEVORASpellWeavingTests.cpp`, `WEVORA.uproject`, `PROGRESS.md` | 自動テスト結果記録、Niagara素材設定、PIE確認 |
| 2026-10-04-02 | テスト用Actorの初期化・終了手順を修正。Editor再ビルド成功、Headless自動テスト3/3成功、diffチェック成功。描画・実入力は対象外。 | 実装済み・未確認 | `Source/WEVORA/Tests/WEVORASpellWeavingTests.cpp`, `PROGRESS.md` | Niagara素材設定、PIE確認、Playerビルド |
| 2026-10-04-03 | コミット・Push依頼に伴い変更範囲、現在ブランチfeature/spell-weaving-prototype、origin、diffチェックを確認。今回の実装・テスト・文書11ファイルをコミット対象とした。 | 調査完了 | 今回の関連11ファイル | コミット・Push、リモートHEAD一致確認 |
| 2026-10-04-04 | 浮遊敵と弾のC++/配置用Blueprint、HealthComponent、生成スクリプト、配置手順、自動テストを追加。Editorビルド成功・Headless 4/4成功。初回のdebug表示キー型のビルドエラーを修正。初回AIテストのGameMode不在によるPlayerState/被弾初期化不足をテストWorldの初期化で修正。BRAINの対象プロジェクト名ファイル検索で関連ノートなし。 | 実装済み・未確認 | `Source/WEVORA/AI/*`, `Source/WEVORA/WEVORACharacter.h/.cpp`, `Source/WEVORA/Tests/WEVORAEnemyTests.cpp`, `Content/WEVORA/AI/*.uasset`, `Scripts/create_enemy_blueprints.py`, `PROGRESS.md` | Editor再起動、配置とPIE確認、Playerビルド |
| 2026-10-04-05 | 選択エフェクトの位置報告を受けC++の取り付け先と初期オフセットを確認。Mesh相対の固定位置で手のソケットへは未接続。希望位置を確認中、位置のコード変更は未実施。 | 調査完了 | `PROGRESS.md` | 希望位置の回答、Blueprint実設定とPIE位置の確認、必要な修正 |
| 2026-10-04-05 | 必要な区切りでのコミット許可を記録。敵AIの関連14ファイルをコミット対象として確認。前回Editorビルド成功・Headless 4/4成功の記録を確認。 | 調査完了 | 敵AIの関連14ファイル | コミット、PIE確認 |
| 2026-10-04-06 | 選択切替は体、編み中は右手に表示する構成へ分離。属性別SelectionSystem、BodyOffset/HandOffset/HandSocketNameを追加。旧Blueprintの固定位置もBeginPlayで右手相対へ置換。初回は開いたEditorのDLLロックでLNK1104、ユーザーが保存・終了後の再ビルド成功。実キャラクターの右手取り付けを含むHeadless魔法4/4成功、テスト警告0。 | 実装済み・未確認 | `Source/WEVORA/Spell/WEVORASpellSelectionEffectComponent.h/.cpp`, `Source/WEVORA/WEVORACharacter.cpp`, `Source/WEVORA/Tests/WEVORASpellWeavingTests.cpp`, `Source/WEVORA/Spell/README.md`, `PROGRESS.md` | Editorを開き直しPIEで体の切替演出と右手追従を確認 |
| 2026-10-04-07 | ユーザーのコミット・Push依頼に伴い、体側の切替演出と右手追従の関連6ファイル、現在ブランチとorigin、diffチェックを確認。直前のEditorビルド成功・魔法テスト4/4成功を引き継ぐ。 | 調査完了 | 今回の関連6ファイル | コミット・Push、リモートHEAD一致確認、PIE確認 |
| 2026-10-05-01 | 飛行改善の操作案を現行Characterの入力・重力・ホバー・加速と照合。無入力時の自然落下、入力によるマナ消費ホバー、静止詠唱時の操作を検討。BRAINのWEVORA対象検索では関連ノートなし。コード変更・ビルド・PIE確認は未実施。 | 調査完了 | `PROGRESS.md` | 静止ホバーとマナ仕様を整理し、実装後PIEで操作感を確認 |
| 2026-10-05-02 | `fix/inertial-mana-flight`を作成。慣性ジャンプ/浮遊/自然落下、マナホバー/上昇、Ctrl優先、Shift強化、Alt静止ホバー、マナ地上回復、入力解除、調整・ユーザー確認手順を追加。初回テストビルドのfloat/double曖昧オーバーロードを修正。初回HeadlessはテストControllerのローカル判定不足で飛行3件失敗、テスト環境を明示ローカル化・EndPlayを追加して飛行4/4成功。最終Editorビルド成功・全9/9成功・テスト警告0。関連9ファイルをコミット対象とし、PIE/実機検証はユーザー担当。 | 実装済み・未確認 | `Source/WEVORA/WEVORACharacter.h/.cpp`, `Source/WEVORA/Movement/*`, `Source/WEVORA/Tests/WEVORAFlightTests.cpp`, `PROGRESS.md` | ユーザーのPIE確認と値調整 |
| 2026-10-05-03 | 共通SpellCastComponent/Projectile/LaunchでFire/Wind認識→空中Cast→実sweep→既存Enemy HP減少を接続。既存最初のマップへ敵1体を保存・再読込確認。仮球/色付きライト、Actor名付き仮HP、操作手順を追加。初回テストビルドのTObjectPtr引数を修正。配置Pythonのbool名を修正。初回戦闘テストはHP減少を確認したがテスト環境にLocalPlayerがなくCameraManager更新が止まり方向/壁確認が失敗、テスト側のカメラ計算を明示して解消。最終Editorビルド成功、Headless全11/11成功・テスト警告0。初回の環境音声警告は今回対象外のため最終は-nosoundで実行。実プレイ/Playerは未確認。 | 実装済み・未確認 | `Source/WEVORA/Spell/WEVORASpellCastComponent.*`, `WEVORASpellProjectile.*`, `WEVORASpellTypes.h`, `Source/WEVORA/WEVORACharacter.*`, `AI/WEVORAHealthComponent.cpp`, `Tests/WEVORASpellCombatTests.cpp`, `Spell/README.md`, `AI/README.md`, `Scripts/place_combat_enemy.py`, `Content/ThirdPerson/Lvl_ThirdPerson.umap`, マップ外部Actor, `PROGRESS.md` | 人間が既存マップで飛行→編み→Cast→命中→HP減少を確認 |


## 2026-10-05 軽量化ベースライン（perf/lean-desktop-baseline）

- main `fafea82` をfetch/fast-forward確認後、専用ブランチを作成。mainへ直接マージしない。
- 製品ベースラインのHardware RT / Lumen Hardware RT / RT Proxy / HWRT Translucent Refraction / Path Tracingを無効化。無効化に伴うRT限定overrideとWindows RHI重複を整理。
- ModelingToolsEditorModeを無効化。Desktopで不要なAndroid File Serverのサービス/ネットワークを無効化し、古いテンプレート名・Editor参照を整理。
- Mac Editor限定のMedium/Low起動、参照監査、LFS実サイズを考慮した比較スクリプトと `Docs/Optimization.md` を追加。
- Niagara / Lumen Software tracing / Mesh Distance Fields / Nanite / VSM / VT / SkyAtmosphere維持。UMG/Slateは現行Controllerで使用しているため維持。コア戦闘C++と全バイナリAssetは変更なし。
- UE_ROOTの実体なし。ユーザーから「ないので、ビルドはスキップで構いません」と回答を受領。Editor Build・既存11テスト・Asset load・Shader compileは未実行。以前の成功記録を今回の成功として扱わない。
- Asset Registryによる参照証明ができないためVariant 591ファイルは未削除。依存するStateTreeモジュール/Plugin/IncludePath維持。Substrate・Volume/Water/RVT/Local Fog等も未使用を証明できず維持。
- 検証: Python構文、Mac起動dry-run、LFS fsck、設定/保護Asset一致、git diffチェック。UE APIの実行互換性は未検証。16GB動作・メモリ/Shader削減数は未測定。
- 次: UE 5.8上で参照監査、Build/全11テスト、Map/Blueprintロードと実RHI Shader検証。人間側で飛行・編み・Cast・命中HP低下と見た目を確認。詳細/再現手順/数値は `Docs/Optimization.md`。

## 2026-10-11 攻撃入力の簡略化

- 通常攻撃をLMB押下・解放だけで発射できるForward弾へ変更。長押しは最大2秒、チャージ率0〜1をContextへ記録し、発射時の基礎Powerに最大2倍を線形乗算。
- 編み攻撃は「先に狙う → E保持で描く → E解放で発射」。LMBとの同時押し・描いた後の発射操作を省略。描画中の照準固定・Q属性切替・トレイル・手元エフェクトを継続。
- 両攻撃は共通0.5秒のクールタイム。期間中の押下は予約せず無視。描画認識失敗は消費なし、認識済み発射試行（壁遮断を含む）は消費。異なる攻撃入力の割り込み、回避・降下・操作解除後の遅延解放による誤発射を防止。
- レティクル下にチャージ量（黄、最大で緑）とクールタイム回復（灰）のゲージを追加。AttackCooldown / MaxChargeTime / MaxChargePowerMultiplierをBlueprintで調整可能。
- 保存済みBlueprintのResolveSpellLaunchが基礎Powerを組み立てるため、チャージ倍率はその後のC++ HandleCastで適用。初回の等倍になる不具合を保存済みBlueprintを使用するテストで検出・修正。
- UE 5.8 / Win64 Development Editorビルド成功。エンジンヘッダーの非推奨API警告あり。初回はビルドキャッシュ書き込みの権限制限に当たり、許可後に再実行。
- Headless最終結果: WEVORA.Spell 12/12成功、全体19/21成功、テスト警告0。失敗はWEVORA.Movement.BurstAndBrakeおよびHeldHoverAndNaturalFall（Shift加速、Altホバー、自然落下等）。飛行側のコード・テストは今回変更せず、失敗の原因と変更前からの再現は未調査。
- 新テストSimpleAttackは短押し、0.5秒の入力制限、1秒半チャージ・2秒上限・最大保持、保存済みBlueprintでの実発射Power、E単独発射、照準固定、重複入力、認識失敗と途中キャンセル後の解放を確認。
- 結果: Saved/Automation/SimpleAttack/index.json、ログ: Saved/Logs/SimpleAttackAutomation.log。Editorの終了コードだけで全成功と判定せずJSON結果を確認。
- PIEの実入力、ゲージの描画・操作感、Playerビルドは未確認。操作手順はSource/WEVORA/Spell/README.md、Blueprintのチャージ適用境界はDocs/BlueprintDesign.mdへ記載。

## 2026-10-14 攻撃・命中エフェクト

- 炎・風・雷を属性別のインスタンスメッシュアニメーションとして実装し、通常/編み攻撃の弾へ接続。色・マテリアル・大きさ・アニメーション速度を編集可能。任意のNiagaraへの置き換えにも対応。
- 命中対象の衝突面マテリアルを優先し、当たり判定用の球に当たった場合は表示メッシュから取得。動的マテリアルも引き継ぐ破片を生成し、弾消滅後も独立して飛散・縮小・自動消去。衝突なし、最大64個/5秒。
- 共通発光マテリアルM_AttackEffectと調整用BP_WEVORASpellProjectileを作成。保存済みプレイヤーBlueprintに雷の選択・発射設定を追加。再実行用Scripts/create_attack_effects.pyとDocs/AttackEffects.mdを追加。
- 雷の標準反応は単発ダメージ。既存の炎・風の反応は保持。
- UE 5.8 Win64 Development Editorビルド成功。共通PCHの読み込み停止を回避するため-NoPCH/-NoSharedPCHで実施し、既存の敵コードにも必要な型の明示includeを追加。エンジンヘッダーの非推奨API警告あり。
- HeadlessテストWEVORA.Spell. 13/13成功。新規AttackEffectsテストで三属性の保存済みBlueprint解決、描画メッシュ生成、対象マテリアル取得、弾消滅後の破片の保持・飛散・寿命・衝突無効を確認。結果Saved/Automation/AttackEffects/index.json。
- 実描画/PIEの見た目、ネットワーク配信、Playerビルドは未確認。

## 2026-10-17 Localized Reactions Stage A

- PR #8 head 6fb59f4からfeature/localized-elemental-reactionsを作成。PR #8/main/既存Assetは変更しない。
- Zone Profile/Resolver/隣接graphを追加。FName IDとBone/Physics Body/Collider設定を分離。未知BoneはNone。
- UE5.8 Editorビルド成功。着手前33/33、Stage A 34/34成功。設計: Docs/LocalizedElementalReactions.md。

## Localized Reactions Stage B

- 汎用局所Statusコンポーネント、4Policy、期限・強度・蓄積・Source/時刻、最大DPS包絡線の区間積分を追加。
- Healthの死亡/reset通知で即時解除。UE5.8 Editorビルド成功、全36/36テスト成功。30/60/120fpsと4秒hitchで20HPの同一結果を検証。

## Localized Reactions Stage C

- FireをHit Zoneへ付与し、Windを同フレームsnapshotで解決。隣接・距離・hop・強度とDataAsset Ruleで伝播を制限。提案mergeで循環/同イベント増幅を防止。
- Deliveryのイベント重複防止とVolumeの複数Zone batchを追加。直撃とWind外力は対象ごと一回、全身/局所燃焼は排他。
- Editorビルド成功、全40/40成功。Fire手→Wind→腕/胴、逆順入力、循環、距離、局所/全身移行、複数Zone/重複callbackを検証。初回は3.9秒比較のfloat許容値が厳しすぎたため0.0001秒に修正。

## Localized Reactions Stage D

- 保存Assetを変更せず6体目の仮8Zone対象をruntime生成。7Collider+既存Torso、局所色変化、F2でZone/寿命/強度/蓄積/経路/HPを表示。既存5対象は維持。
- Editorビルド成功、全42/42成功。実Projectileの右手Fire→隣接腕Wind→胴体伝播/外力、実Volumeの手のみ/8Zone同時命中、死亡復活/全体reset、F2判定不変を確認。

## Localized Reactions Stage E（最終）

- 調整用Zone/FireWind DataAssetを新規2件作成し、仮対象へ接続。作成スクリプトは既存名への上書きを拒否。既存Map/Blueprint/Configは保存し直さない。
- 同frameの129入力でもPower等による正規化で順序不変、状態batch確定後の通知、弱いrefresh/複数seedでの最大DPS Source帰属、不正Ruleの拒否を補強。
- 最終UE5.8.3 Win64 Development Editor/Playerビルド成功。全45/45成功（既存33の名前・実装を維持、新規12）、未実行・テスト警告・エラー0。BP解決の3属性×4Shape×通常/最大Chargeの24組合せを実配送で検証。
- 全Blueprint349件（/Game55件）コンパイル成功、エラー/警告0。2マップCook/Stage/Pak/Package/Archive成功（最終UAT 64秒、終了0）。最終Archive exeはBinaries版とSHA-256一致。
- 最終PackageのD3D12/SM6でPlaygroundロード、452フレーム・終了0。F2パネル/仮Zone対象の初期ScreenshotをDocs/Images/LocalizedZones.pngへ保存。初期Trial 0であり、実入力や延焼中の表示の証明にしない。
- 既存Content/Config770ファイルの前後SHA-256一致、新規DataAssetを含め772件も一致。最終Cook後に既存Content765件をbaseのLFS SHA-256と照合、Config差分なし。LFS fsck/diffチェック/PowerShell構文検証成功。
- Engine非推奨/推奨MSVC差、既存RigVM EditorDomain循環、NVIDIA TSRドライバー/初回PSOのメッセージは残る。自動テスト/Blueprintの警告とは区別する。
- Raw結果: Saved/Automation/LocalizedFinal/index.json。共有用: Docs/LocalizedElementalAutomationResults.json。ログ: Saved/Logs/LocalizedFinal*。配布物: Saved/LocalizedReactionsPlayer/Windows/WEVORA.exe。
- 調整値・PIE10手順・責務/互換性/未実装: Docs/LocalizedElementalReactions.md。再検証: Scripts/validate_localized_reactions.ps1（-SkipPlayer対応）。ROADMAP23〜34とGAME_DESIGNへ試作範囲を反映。
- 人間の実入力/飛行射撃/狙いやすさ/動的な延焼表示/30・60・120fps体感/面白さと正式Skeleton/mGear衝突は未確認。水/Lightning新状態/地形伝播/Replication/正式演出は実装しない。
- PR #8はDraft/open、head 6fb59f4のまま。main・PR #8は変更/マージせず、本ブランチだけをStacked Draft PRにする。
