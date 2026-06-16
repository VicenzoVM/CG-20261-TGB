# Trabalho Pratico - Grau A

Leitor e visualizador de cenas 3D com OpenGL moderna. O projeto carrega modelos OBJ triangulados, exibe multiplos objetos, permite selecionar e transformar cada objeto, controla camera FPS e aplica iluminacao de Phong.

## Componentes

- Arthur Palma e Vicenzo Valmórbida

## Requisitos

- CMake 3.10 ou superior
- Compilador C++ com suporte a C++17
- OpenGL
- Git, usado pelo CMake para baixar GLFW, GLM, nlohmann/json e stb no primeiro configure

## Compilacao

```bash
cmake -S . -B build
cmake --build build --target TrabalhoGA
```

## Execucao

Execute a partir da pasta `build`. Por padrao, o programa carrega a cena em `../assets/cena.json`.

```bash
cd build
./TrabalhoGA
```

Tambem e possivel informar outro arquivo de cena:

```bash
./TrabalhoGA ../assets/outra_cena.json
```

## Arquivo de cena

A cena e definida em `assets/cena.json`. O arquivo contem camera, projecao/frustum, luzes e objetos. Os caminhos dos modelos sao relativos ao proprio arquivo de cena.

Campos principais:

- `camera`: `position`, `up`, `yaw`, `pitch`.
- `projection`: `type`, configuracao `perspective` e configuracao `orthographic`.
- `lights`: lista de luzes pontuais com `position` e `color`.
- `objects`: lista de objetos com `name`, `file`, `position`, `rotation`, `scale`, `color` e `selectedColor`.

O carregador tambem aceita alguns aliases em portugues usados na especificacao, como `arquivo`, `posicao`, `trans`, `rot`, `rotacao`, `escala` e `cor`.

## Controles

### Camera e janela

- `W`, `A`, `S`, `D`: movimenta a camera em primeira pessoa.
- Mouse: rotaciona a camera.
- `P`: alterna entre projecao perspectiva e ortografica.
- `Esc`: fecha a janela.

### Objeto selecionado

- `Tab`: alterna entre os objetos carregados no arquivo de cena.
- `Seta esquerda` / `Seta direita`: translada no eixo X.
- `Seta cima` / `Seta baixo`: translada no eixo Y.
- `O` / `L`: translada no eixo Z.
- `X`, `Y`, `Z`: rotaciona no respectivo eixo.
- `E` / `R`: aumenta ou diminui a escala uniforme.
- `1` / `2`: aumenta ou diminui a escala no eixo X.
- `3` / `4`: aumenta ou diminui a escala no eixo Y.
- `5` / `6`: aumenta ou diminui a escala no eixo Z.

### Visualizacao, luz e material

- `F`: liga/desliga o wireframe sobreposto ao solido.
- `Shift + setas`: move a luz pontual nos eixos X e Y.
- `Shift + O` / `Shift + L`: move a luz pontual no eixo Z.
- `B`: alterna o parametro de material selecionado na ordem `ka`, `kd`, `ks`, `q`; o parametro selecionado aparece no titulo da janela.
- `N` / `M`: diminui ou aumenta o parametro de material selecionado e atualiza o valor no titulo da janela.
