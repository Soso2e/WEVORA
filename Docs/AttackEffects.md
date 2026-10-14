# 攻撃・命中エフェクト

Qで炎 → 風 → 雷を選択。通常攻撃・ジェスチャー攻撃のどちらも、発射した弾の
`AttackEffect`コンポーネントが属性の見た目を生成する。
炎は後方に細く消える火の粒、風は回転する三本の螺旋、雷は形が瞬間的に変わる三本の稲妻。
共通の発光マテリアルは `/Game/WEVORA/Effects/M_AttackEffect`。

## 見た目の調整

- 色: キャラクターの `SpellCastComponent / Element Parameters / Color`。
- `/Game/WEVORA/Effects/BP_WEVORASpellProjectile` の `AttackEffect / Element Effects`:
  属性別のMaterial、Scale、Animation Speed。
- マテリアルのベクトルパラメータ `SpellColor` に選択した色を渡す。
  共通マテリアルの `EmissiveIntensity` で発光の強さを調整できる。
- `Use Launch Color` を無効にすると、Element Effects内のColorを使う。
- `System` にNiagaraを指定すると、その属性の標準メッシュアニメーションを置き換える。
  `User.SpellColor` (Linear Color)、`User.SpellRadius` (Float)、
  `User.SpellMaterial` (Material) を受け取るようNiagara側を設定する。

## 命中時の破片

`AttackEffect / Impact` で破片のモデル、個数、サイズ、速度、重力、寿命を変更できる。
標準は小さな立方体の破片12個、寿命0.8秒。終盤に縮小して消える。
環境の壁など、ダメージを受け取らないオブジェクトへの命中でも発生する。

命中メッシュの衝突面マテリアルを優先して使用する。球などの当たり判定に命中した
場合は、対象の表示メッシュから取得する。面を特定できない場合はスロット0、
表示メッシュがない場合はFallback Materialを使用する。
動的マテリアルインスタンスもそのまま使用するため、対象の色・質感が破片に反映される。
複数の表示メッシュがある場合は命中位置に最も近いメッシュを選ぶ。

破片は描画用の弾道アニメーションで、他の攻撃や移動を妨げる衝突は持たない。
描画は攻撃・命中ごとにインスタンス化してまとめ、破片数は最大64、寿命は最大5秒に制限。
Dedicated Serverでは生成しない。現在のローカル攻撃処理に接続しており、
ネットワークへのエフェクト配信は含まない。

## 再生成・検証

`Scripts/create_attack_effects.py` は共通マテリアルを作成し、保存済みプレイヤーBlueprintに
雷の選択・発射設定を追加する。既存の炎・風の設定やマテリアルグラフは保持する。
新しいC++の初回ビルド前は `-EffectsMaterialOnly` でマテリアルのみ作成できる。

自動テスト: `WEVORA.Spell.Combat.AttackEffects`。
三属性のBlueprint発射解決、表示メッシュからのマテリアル取得、破片の独立した寿命、
衝突無効、飛散を検証する。実際の発光や見た目の確認はエディタのPlayで行う。

2026-10-14検証結果: UE 5.8 Win64 Development Editorビルド成功、
`WEVORA.Spell.` 13/13成功。結果は `Saved/Automation/AttackEffects/index.json`。
共通PCHの読み込みでコンパイルが進まなかったため、今回のビルドは
`-NoPCH -NoSharedPCH -NoUBA -UBANoDetour -nocache -MaxParallelActions=4` を付けて実施。
実描画・PIEの見た目とPlayer向けビルドは未確認。
