#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>


#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 900
#define OFFSETX WINDOW_WIDTH/2
#define OFFSETY WINDOW_HEIGHT/2
#define SCALE 600
#define PRESPECTIVE 3.0

#define RADIUS 1
#define NPAR 12
#define NMER 24



typedef int Index;

typedef struct{
  double theta;
  double phi;
}Polar2;

typedef struct{
  double x;
  double y;
}Vec2;

typedef struct{
  double x;
  double y;
  double z;
}Vec3;

typedef struct{
  Index start;
  Index end;
}Connection;

typedef struct{
  Vec3 *points;
  int nPoints;
  Connection *connections;
  int nConnections;
  double xRotationAngle;
  double yRotationAngle;
}Model;

typedef struct{
  double radius;
  Model model;
}Sphere;


// parametri per motore grafico
double globalScaling = SCALE;
double xOffset = OFFSETX;
double yOffset = OFFSETY;


//oggetti SDL
SDL_Window* window;
SDL_Renderer* renderer;
SDL_Event event;



Vec3 fromPolarToCartesian(Polar2);
Polar2 fromCartesianToPolar(Vec3);
void calculateSphereModel(Sphere*);
void calculateSpherePoints(int, Vec3*);
void calculateSphereConnections(int, Connection*);
void drawSphere(Sphere*,bool);
Vec2 projectsP3(Vec3);
void drawSystemVector(Polar2,double,double);
void drawThickLine(SDL_Renderer*,Vec2,Vec2,int);
Vec3 rotateYVec3(Vec3,double);
Vec3 rotateXVec3(Vec3,double);
Vec3 rotateZVec3(Vec3,double);
void applyXGate(Polar2*);
void applyYGate(Polar2*);
void applyZGate(Polar2*);
void resetState(Polar2*);
void apply45RotationGate(Polar2*,char);


int main(void){
  bool execution = true;
  Polar2 systemState = {0,0};
  //modello sfera
  Sphere sphereData = {1,{NULL,NPAR*NMER,NULL,(NPAR*NMER) + ((NPAR-1)*NMER),0,0}};
  sphereData.model.points = malloc(sphereData.model.nPoints*sizeof(Vec3));
  sphereData.model.connections = malloc(sphereData.model.nConnections*sizeof(Connection));
  if(sphereData.model.points == NULL || sphereData.model.connections == NULL){
    printf("error while allocating the sphere model\n");
    return 1;
  }
  calculateSphereModel(&sphereData);

  double rotationSense = 0.01;
  Uint32 flags;
  int actualWidth;
  int actualHeight;

  if (SDL_Init(SDL_INIT_VIDEO) != 0){
    printf("error while initializing SDL: %s\n",SDL_GetError());
    free(sphereData.model.points);
    free(sphereData.model.connections);
    return 1;
  }

  window = SDL_CreateWindow(
    "bloch sphere",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    WINDOW_WIDTH,
    WINDOW_HEIGHT,
    0
  );


  if(window == NULL){
    printf("error while creating the window: %s\n",SDL_GetError());
    SDL_Quit();
    free(sphereData.model.points);
    free(sphereData.model.connections);
    return 1;
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

  if (renderer == NULL) {
    printf("error while creating the renderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(sphereData.model.points);
    free(sphereData.model.connections);
    return 1;
  }
  
  printf("initial system state theta: %.2f, phi: %.2f\n",systemState.theta,systemState.phi);
  while(execution){

    //event handling
    while(SDL_PollEvent(&event)){
      if(event.type == SDL_QUIT){
        execution = false;
      }else if(event.type == SDL_KEYDOWN){

        switch(event.key.keysym.sym){
          case SDLK_ESCAPE:
            execution = false;
            break;
          case SDLK_x:
            if(event.key.keysym.mod & KMOD_SHIFT){
              apply45RotationGate(&systemState,'X');
            }else applyXGate(&systemState);
            printf("new system state theta: %.2f, phi: %.2f\n",systemState.theta,systemState.phi);
            break;
          case SDLK_y:
            if(event.key.keysym.mod & KMOD_SHIFT){
              apply45RotationGate(&systemState,'Y');
            }else applyYGate(&systemState);
            printf("new system state theta: %.2f, phi: %.2f\n",systemState.theta,systemState.phi);
            break;
          case SDLK_z:
            if(event.key.keysym.mod & KMOD_SHIFT){
              apply45RotationGate(&systemState,'Z');
            }else applyZGate(&systemState);
            printf("new system state theta: %.2f, phi: %.2f\n",systemState.theta,systemState.phi);
            break;
          case SDLK_r:
            resetState(&systemState);
            printf("new system state theta: %.2f, phi: %.2f\n",systemState.theta,systemState.phi);
            break;
          case SDLK_f:
            flags = SDL_GetWindowFlags(window);
            if(flags & SDL_WINDOW_FULLSCREEN_DESKTOP){
              SDL_SetWindowFullscreen(window, 0);
            }
            else{
              SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
            SDL_GetWindowSize(window, &actualWidth, &actualHeight);
            xOffset = actualWidth /2.0;
            yOffset = actualHeight / 2.0;
            break;
          default:
            break;
        }

      }else if(event.type == SDL_MOUSEMOTION){
        if(event.motion.state & SDL_BUTTON_LMASK){
          sphereData.model.yRotationAngle -= event.motion.xrel * rotationSense;
          sphereData.model.xRotationAngle -= event.motion.yrel * rotationSense;

          sphereData.model.yRotationAngle = fmod(sphereData.model.yRotationAngle, 2 * M_PI);
          if (sphereData.model.yRotationAngle < 0) {
            sphereData.model.yRotationAngle += 2 * M_PI;
          }

          sphereData.model.xRotationAngle = fmod(sphereData.model.xRotationAngle, 2 * M_PI);
          if (sphereData.model.xRotationAngle < 0) {
            sphereData.model.xRotationAngle += 2 * M_PI;
          }
        }
      }


    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0,0, 255);
    SDL_RenderClear(renderer);
    
    SDL_SetRenderDrawColor(renderer, 70, 70, 70, 255);
    drawSphere(&sphereData,false);
    

    SDL_SetRenderDrawColor(renderer, 0,0,255,255);
    drawSystemVector(systemState,sphereData.model.xRotationAngle,sphereData.model.yRotationAngle);
    
    SDL_SetRenderDrawColor(renderer, 255,255,255,125);
    drawSphere(&sphereData,true);
    

    SDL_RenderPresent(renderer);


  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);

  SDL_Quit();
  free(sphereData.model.points);
  free(sphereData.model.connections);
  return EXIT_SUCCESS;

}


Vec3 fromPolarToCartesian(Polar2 polarCoordinates){
  Vec3 cartesianCoordinates;

  cartesianCoordinates.y = cos(polarCoordinates.theta);
  cartesianCoordinates.x = sin(polarCoordinates.theta) * cos(polarCoordinates.phi);
  cartesianCoordinates.z = sin(polarCoordinates.theta) * sin(polarCoordinates.phi);
  return cartesianCoordinates;
}

Polar2 fromCartesianToPolar(Vec3 cartesianCoordinates){
  double y_val = cartesianCoordinates.y;
  if(y_val > 1.0) y_val = 1.0;
  if(y_val < -1.0) y_val = -1.0;
  double theta = acos(y_val);
  double phi = atan2(cartesianCoordinates.z,cartesianCoordinates.x);
  Polar2 polarCoordinates;
  if (phi < 0) {
    phi += 2.0 * M_PI;
  }
  polarCoordinates.theta = theta;
  polarCoordinates.phi = phi;
  return polarCoordinates;

}

void calculateSphereModel(Sphere *sphere){
  calculateSpherePoints(sphere->model.nPoints,sphere->model.points);
  calculateSphereConnections(sphere->model.nConnections, sphere->model.connections);
}

void calculateSpherePoints(int nPoints, Vec3* points){
  Polar2 polarCoordinates = {0,0};
  double passoTheta = M_PI/(NPAR - 1);
  double passoPhi = (2 * M_PI)/(NMER);
  Index pointIndex = 0;
  for(int i = 0; i < NPAR; i++){
    polarCoordinates.theta = passoTheta*i;
    for(int j = 0; j < NMER; j++){
      polarCoordinates.phi = passoPhi * j;
      points[pointIndex] = fromPolarToCartesian(polarCoordinates);
      pointIndex++;
    }
  }
}


void calculateSphereConnections(int nConnections,Connection *connections){
  Index connectionIndex = 0;
  Index currentPoint;
  Index nextPoint;
  for(int i = 0; i < NPAR; i++){
    for(int j = 0; j < NMER; j++){
      currentPoint = (i*NMER) + j;

      nextPoint = (i * NMER) + ((j+1)%NMER);
      connections[connectionIndex].start = currentPoint;
      connections[connectionIndex].end = nextPoint;
      connectionIndex++;


      if (i < NPAR -1){
        nextPoint = ((i+1)*NMER)+j;
        connections[connectionIndex].start = currentPoint;
        connections[connectionIndex].end = nextPoint;
        connectionIndex++;
      }
    }
  }
}


void drawSphere(Sphere *sphere,bool drawFront){
  Vec2 projectedPoints[sphere->model.nPoints];
  Vec3 rotatedPoints[sphere->model.nPoints];
  Connection tmpC;
  Vec2 p1;
  Vec2 p2;
  for(int i = 0;i < sphere->model.nPoints; i++){
    rotatedPoints[i] = rotateXVec3(rotateYVec3(sphere->model.points[i],sphere->model.yRotationAngle),sphere->model.xRotationAngle);
    projectedPoints[i] = projectsP3(rotatedPoints[i]);
  }
  for(int i = 0; i < sphere->model.nConnections; i++){
    tmpC = sphere->model.connections[i];
    p1 = projectedPoints[tmpC.start];
    p2 =  projectedPoints[tmpC.end];


    double z1 = rotatedPoints[tmpC.start].z;
    double z2 = rotatedPoints[tmpC.end].z;
    double zMid = (z1 + z2) / 2.0;

    if(drawFront && zMid > 0) continue;
    
    if(!drawFront && zMid <= 0) continue;
    
    SDL_RenderDrawLine(renderer,p1.x,p1.y,p2.x,p2.y);
  }
}


Vec2 projectsP3(Vec3 point){
  double x2D = point.x/(point.z+PRESPECTIVE);
  double y2D = point.y/(point.z+PRESPECTIVE);
  x2D = round((x2D*globalScaling)+xOffset);
  y2D = round(yOffset - (y2D * globalScaling));
  Vec2 pointRet = {x2D,y2D};
  return pointRet;
}

void drawSystemVector(Polar2 systemState,double xRotationAngle, double yRotationAngle){
  Vec3 cartesianPoint = fromPolarToCartesian(systemState);
  Vec2 pPoint = projectsP3(rotateXVec3(rotateYVec3(cartesianPoint,yRotationAngle),xRotationAngle));
  Vec2 origin = {xOffset,yOffset};
  SDL_Rect systemRect = {
    .x = pPoint.x-1,
    .y = pPoint.y-1,
    .w = 3,
    .h = 3
  };
  SDL_RenderFillRect(renderer, &systemRect);
  drawThickLine(renderer,origin,pPoint,3);

}

void drawThickLine(SDL_Renderer *renderer,Vec2 p1, Vec2 p2, int thickness){
  if(thickness == 0){
    return;
  }
  
  double dx = fabs(p2.x - p1.x);
  double dy = fabs(p2.y - p1.y);

  if(dx > dy){
    SDL_RenderDrawLine(renderer, p1.x,p1.y+thickness-1,p2.x,p2.y+thickness-1);
    SDL_RenderDrawLine(renderer, p1.x,p1.y-(thickness-1),p2.x,p2.y-(thickness-1));
  }else{
    SDL_RenderDrawLine(renderer, p1.x+thickness-1,p1.y,p2.x+thickness-1,p2.y);
    SDL_RenderDrawLine(renderer, p1.x-(thickness-1),p1.y,p2.x-(thickness-1),p2.y);
  }


  drawThickLine(renderer,p1,p2,thickness-1);
}

Vec3 rotateYVec3(Vec3 point, double angle){
  double xNew = (point.x * cos(angle)) + (point.z * sin(angle));
  double zNew = (-1*(point.x * sin(angle))) + (point.z * cos(angle));
  Vec3 pointRotated = {xNew,point.y,zNew};
  return pointRotated;
}
Vec3 rotateXVec3(Vec3 point,double angle){
  double yNew = (point.y * cos(angle)) - (point.z * sin(angle));
  double zNew = (point.y * sin(angle)) + (point.z * cos(angle));
  Vec3 pointRotated = {point.x,yNew,zNew};
  return pointRotated;
}

Vec3 rotateZVec3(Vec3 point,double angle){
  double xNew = (point.x * cos(angle)) - (point.y * sin(angle));
  double yNew = (point.x * sin(angle)) + (point.y * cos(angle));
  Vec3 pointRotated = {xNew,yNew,point.z};
  return pointRotated;
}


void applyZGate(Polar2 *systemState){
  systemState->phi += M_PI;
  systemState->phi = fmod(systemState->phi, 2*M_PI);
}

void applyXGate(Polar2 *systemState){
  systemState->theta = M_PI - systemState->theta;
  systemState->phi = -1 * systemState->phi;
  if(systemState->phi < 0){
    systemState->phi+=2*M_PI;
  }
}

void applyYGate(Polar2* systemState){
  systemState->theta = M_PI - systemState->theta;
  systemState->phi =M_PI - systemState->phi;
  if(systemState->phi < 0){
    systemState->phi+=2*M_PI;
  }
}
void resetState(Polar2 * systemState){
  systemState->theta = 0.0;
  systemState->phi = 0.0;
}

void apply45RotationGate(Polar2 *systemState,char axis){
  Polar2 newState;
  Vec3 cartesianCoordinates = fromPolarToCartesian(*systemState);
  switch(axis){
    case 'X':
      cartesianCoordinates = rotateXVec3(cartesianCoordinates,M_PI/4);
      break;
    case 'Y':
      cartesianCoordinates = rotateZVec3(cartesianCoordinates,M_PI/4);
      break;
    case 'Z':
      cartesianCoordinates = rotateYVec3(cartesianCoordinates,M_PI/4);
      break;
    default:
      return;
  }

  newState = fromCartesianToPolar(cartesianCoordinates);
  systemState->theta = newState.theta;
  systemState->phi = newState.phi;
}
