(ns clj-openkinect.core
  (:import [org.openkinect.freenect Freenect Context Device]))

(defn tilt [angle]
  (let [context (Freenect/createContext)
		dev (. context openDevice 0)]
		(. dev setTiltAngle angle)
		(. context shutdown)))

  
