================================================================================
                    CRYSTAL CAVES - 3D MODEL GUIDE
================================================================================

HOW TO ADD YOUR 3D MODELS:
--------------------------

1. Place your .OBJ files in this "models" folder

2. If your model has materials, include the .MTL file in the same folder

3. If your model has textures, include the texture images (.png, .jpg) here too


SUPPORTED FILE FORMATS:
-----------------------
- .OBJ (Wavefront OBJ) - PRIMARY FORMAT
- .MTL (Material Library) - For materials/colors


EXAMPLE FOLDER STRUCTURE:
-------------------------
models/
├── README.txt (this file)
├── cave_entrance.obj
├── cave_entrance.mtl
├── crystal_blue.obj
├── crystal_blue.mtl
├── gem.obj
├── coin.obj
├── stalactite.obj
└── textures/
    ├── cave_diffuse.png
    └── crystal_normal.png


HOW TO LOAD MODELS IN CODE:
---------------------------
In crystalcaves.cpp, in the Scene init() function:

    // Load a model
    OBJModel* myModel = modelManager.loadModel("model_name", "models/mymodel.obj");
    
    // Set position, rotation, scale
    if (myModel) {
        myModel->setPosition(x, y, z);
        myModel->setRotation(rx, ry, rz);  // degrees
        myModel->setUniformScale(1.0f);
        addModel(myModel);
    }


RECOMMENDED MODELS TO CREATE:
-----------------------------
Based on your Crystal Caves proposal:

ENVIRONMENT:
  □ cave_entrance.obj     - Main cave entrance environment
  □ deep_cave.obj         - Deep cavern environment
  □ cave_wall.obj         - Modular wall sections
  □ stalactite.obj        - Hanging rock formations
  □ stalagmite.obj        - Ground rock formations

CRYSTALS:
  □ crystal_small.obj     - Small crystal (collectible)
  □ crystal_medium.obj    - Medium crystal (decoration)
  □ crystal_large.obj     - Large crystal formation
  □ crystal_cluster.obj   - Group of crystals

COLLECTIBLES:
  □ gem_red.obj           - Red gem (points)
  □ gem_blue.obj          - Blue gem (points)
  □ gem_green.obj         - Green gem (points)
  □ gem_purple.obj        - Rare purple gem (bonus)
  □ coin.obj              - Gold coin
  □ key.obj               - Key item (if needed)
  □ powerup.obj           - Power-up item

OBSTACLES/HAZARDS:
  □ rock_pile.obj         - Blocking rocks
  □ spike.obj             - Dangerous spike
  □ falling_rock.obj      - Falling hazard

PLAYER (if applicable):
  □ player.obj            - Player character
  □ torch.obj             - Light source


TIPS FOR MODEL CREATION:
------------------------
1. Keep polygon count reasonable (< 5000 triangles per model)
2. Center your models at origin (0, 0, 0)
3. Use Y-up orientation (standard for most 3D software)
4. Export with normals included
5. Use simple, solid colors in materials for best results
6. Scale models appropriately (1 unit = 1 meter suggested)


BLENDER EXPORT SETTINGS:
------------------------
If using Blender to create/export models:
  - File > Export > Wavefront (.obj)
  - Check "Include Normals"
  - Check "Write Materials"  
  - Check "Triangulate Faces" (optional, but recommended)
  - Set Forward: -Z Forward
  - Set Up: Y Up


================================================================================
