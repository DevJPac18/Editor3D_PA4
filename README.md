# Editor 3D – Proyecto PA4 (C++ / OpenGL 2.0)

Herramienta para **construir y editar escenas 3D** (no una escena fija en el código):
crear objetos, transformarlos con gizmos, organizarlos en **jerarquías**, asignar
**materiales y texturas**, importar **modelos OBJ**, configurar **animaciones**,
reproducirlas y **guardar / cargar** el trabajo.

Incluye un **brazo robótico articulado** (6 grados de libertad + pinza) y un
**sistema solar** (órbitas anidadas) para demostrar el movimiento jerárquico.

## Compilar y ejecutar

Requisitos: Windows + MinGW g++ (probado con GCC 6.3) o cualquier g++/MSVC con CMake.
No hay dependencias externas: sólo Win32, OpenGL 1.1/2.0 y GLU del sistema.

```bat
build.bat            :: genera bin\Editor3D.exe
build.bat debug      :: versión con símbolos de depuración
```

Con CMake: `cmake -S . -B build -G "MinGW Makefiles" && cmake --build build`.

El ejecutable busca la carpeta `assets/` junto a él o en directorios superiores.
Al iniciar se generan las texturas procedurales en `assets/textures/` si faltan.
`tools/gen_models.py` regenera los modelos OBJ de ejemplo (peón, jarrón, engranaje).

## Funcionalidades

| Requisito | Implementación |
|---|---|
| Escena / objetos | Grafo de escena (`SceneNode`): mallas, luces, grupos vacíos y articulaciones |
| Transformaciones | Posición, rotación (Euler Y·X·Z) y escala; gizmos para mover, rotar y escalar con ajuste por pasos (Ctrl) |
| Jerarquía | Arrastrar y soltar en el árbol o combo «Padre» (se conserva la transformación global) |
| Articulaciones | Rotación o traslación sobre un eje local con límites; deslizadores de todo el modelo |
| Cámara | Orbital: orbitar, desplazar, zoom, enfocar, vistas frontal/lateral/superior |
| Render | OpenGL 2.0: shader GLSL 1.10 Phong por píxel (con alternativa al pipeline fijo), sombras planas con stencil, contorno de selección |
| Materiales | Color, ambiental, difusa, especular, brillo, emisión |
| Iluminación | Hasta 2 luces (puntual / direccional) + luz ambiental |
| Texturas | BMP/TGA con mipmaps, repetición; 11 texturas procedurales incluidas |
| Modelos | Cargador OBJ (v/vt/vn/f, polígonos, índices negativos, normales automáticas) |
| Animación | Pistas por propiedad: inicio/fin, duración, retardo, repetir, ida y vuelta, suavizado |
| Persistencia | Formato de texto `.e3d` (nodos, materiales, articulaciones, animaciones, cámara) |
| Edición | Deshacer/rehacer, duplicar (con hijos y animaciones), eliminar, arrastrar archivos a la ventana |

## Estructura modular

```
src/
  core/      Math3D (vectores, matrices, descomposición TRS), Utils (UTF-8, archivos)
  scene/     SceneNode, Scene (grafo y jerarquía), SceneFactory (primitivas, robot, demo)
  anim/      Animation (pistas e interpolación), Animator (play/pausa/stop)
  render/    GL/GLExt (carga de OpenGL 2.0), Mesh (primitivas), Texture (+TextureGen),
             Shader (GLSL), Camera, Renderer (luces, sombras, selección, picking)
  io/        ObjLoader, SceneIO (.e3d)
  ui/        Gui: interfaz de modo inmediato dibujada con OpenGL
  editor/    Editor (lógica, viewport, historial), EditorPanels (paneles), Gizmo
  platform/  FileDialog (diálogos nativos)
  main.cpp   Ventana Win32 + contexto WGL + bucle principal
```

## Controles

- Clic: seleccionar · Doble clic: enfocar · Clic derecho / Alt+clic: orbitar · Botón medio / Shift+derecho: desplazar · Rueda: zoom
- Q/W/E/R: seleccionar/mover/rotar/escalar · L: ejes locales/globales · Ctrl al arrastrar: pasos
- F enfocar · P padre · H ocultar · Supr eliminar · Ctrl+D duplicar · Ctrl+Z/Y deshacer/rehacer
- Ctrl+N/O/S: nueva/abrir/guardar · Espacio: reproducir/pausar · 1/3/7: vistas · F1: ayuda

## Brazo robótico

`Robot > Cintura (Y) > Hombro (Z) > Codo (Z) > Muñeca (Z) > Rotor (Y) > DedoIzq/DedoDer (traslación X)`.
Cada articulación es un pivote vacío; las piezas visibles cuelgan de él, de modo que rotar un
pivote mueve toda la cadena que depende de él. Selecciona cualquier pieza para ver los deslizadores de
todas las articulaciones, o usa *Animación > Demo: animar brazo robótico*.

## Nota técnica

Con GCC 6.x de MinGW (32 bits), `-O2` genera clones IPA-CP de métodos `thiscall` que limpian
mal la pila. El script de compilación usa `-fno-ipa-cp` para evitar ese fallo.
