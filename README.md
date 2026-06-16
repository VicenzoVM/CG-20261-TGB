# Trabalho do Grau B

Visualizador de uma cena 3D em OpenGL moderna. O programa carrega uma cena definida em JSON, renderiza modelos OBJ triangulados com materiais MTL e texturas, exibe multiplas luzes pontuais com iluminacao de Phong, controla uma camera em primeira pessoa e permite selecionar, transformar e animar objetos.

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
cmake --build build --target TrabalhoGB
```

## Execucao

O executavel gerado se chama `TrabalhoGB`. Por padrao, o programa procura a cena em `../assets/cena.json` ou `assets/cena.json`, dependendo da pasta de onde ele for executado.

```bash
cd build
./TrabalhoGB
```

Tambem e possivel informar outro arquivo de cena:

```bash
./TrabalhoGB ../assets/outra_cena.json
```

## Arquivo de cena

A cena padrao e definida em `assets/cena.json`. Atualmente ela monta um cenario de ilha/forte pirata com oceano, areia, muralhas, navios, canhoes, bau, objetos de carga e uma bala de canhao animada. Os caminhos dos modelos sao relativos ao proprio arquivo de cena.

Campos principais:

- `camera`: `position`, `up`, `yaw`, `pitch`.
- `projection`: `type`, configuracao `perspective` e configuracao `orthographic`.
- `lights`: lista de luzes pontuais com `type`, `position` e `color`. O shader usa ate 8 luzes.
- `objects`: lista de objetos com `name`, `file`, `position`, `rotation`, `scale`, `color`, `selectedColor` e, opcionalmente, `trajectory`.

O carregador tambem aceita alguns aliases em portugues usados na especificacao, como `nome`, `arquivo`, `posicao`, `trans`, `rot`, `rotacao`, `escala`, `cor`, `corSelecionado` e `centro`.

### Modelos, materiais e texturas

O carregador de OBJ usa vertices (`v`), coordenadas de textura (`vt`), normais (`vn`), bibliotecas de material (`mtllib`) e troca de material (`usemtl`). As faces precisam estar trianguladas; faces com mais ou menos de tres vertices sao ignoradas.

Arquivos MTL podem definir `Ka`, `Kd`, `Ks`, `Ns` e `map_Kd`. Quando ha textura difusa (`map_Kd`), ela e aplicada ao objeto; caso contrario, o programa usa `color` ou `selectedColor` do JSON.

### Trajetorias

Objetos podem ter uma propriedade `trajectory`:

- `type: "anchored"`: aplica uma oscilacao leve de altura, pitch e roll, usada nos barcos ancorados.
- `type: "circular"`: move o objeto em torno de `center` com `radius` e `duration`.
- `type: "bezier"`: move o objeto por uma curva Bezier usando `points` e `duration`.

Objetos animados por trajetoria circular ou Bezier tambem ajustam a rotacao no eixo Y para acompanhar a direcao do movimento.

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

### Visualizacao e luz

- `F`: liga/desliga o wireframe sobreposto ao solido.
- `Shift + setas`: move a primeira luz pontual nos eixos X e Y.
- `Shift + O` / `Shift + L`: move a primeira luz pontual no eixo Z.
